//! kopuz-device — standalone music player firmware.
//!
//! Architecture:
//!   * C components (components/*) own the metal: e-paper SPI driver, button
//!     GPIO/ISR, and the PWM/I2S audio sink. Exposed to Rust via esp-idf-sys
//!     bindgen (see src/ffi.rs).
//!   * Rust owns the high level: SD library scan, audio decode pipeline, the
//!     mini-player UI, and the app state machine.
//!
//! Threads:
//!   main  — poll buttons, mutate App, repaint the e-paper on change.
//!   audio — decode the current track to PCM, feed the C audio sink, advance
//!           the queue on end-of-stream.

mod app;
mod art;
mod audio;
mod display;
mod ffi;
mod library;
mod sd;

use app::{App, Command, PlaybackState};
use audio::decoder;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::time::Duration;

// How often to repaint while playing (loop tick = 50ms). The TFT is fast so
// the clock/progress can update ~every second; e-paper refresh is slow, so it
// stays at ~4s to avoid constant flicker.
#[cfg(feature = "ili9341")]
const REPAINT_TICKS: u32 = 20;
#[cfg(not(feature = "ili9341"))]
const REPAINT_TICKS: u32 = 80;

fn main() -> anyhow::Result<()> {
    esp_idf_svc::sys::link_patches();
    esp_idf_svc::log::EspLogger::initialize_default();
    log::info!("kopuz-device booting");

    ffi::buttons::init();
    ffi::battery::init();
    let audio_sink = audio::Audio::new();
    let mut display = display::Display::new();
    log::info!("init done");

    log::info!("audio test tone");
    {
        let mut tone = [0i16; audio::SAMPLE_RATE as usize / 10 * 2];
        let half = (audio::SAMPLE_RATE / 440 / 2) as usize;
        for (i, frame) in tone.chunks_mut(2).enumerate() {
            let v = if (i / half) % 2 == 0 { 16000 } else { -16000 };
            frame[0] = v;
            frame[1] = v;
        }
        for _ in 0..5 {
            audio_sink.play_block(&tone);
        }
        audio_sink.pause();
    }

    display::mini_player::render_message(display.frame(), "KOPUZ", "starting...");
    display.flush();

    sd::mount()?;
    let queue = library::scan(library::MOUNT_POINT).unwrap_or_default();
    let app = Arc::new(Mutex::new(App::new(queue)));
    app.lock().unwrap().battery_mv = ffi::battery::read_mv().unwrap_or(-1);

    let (tx, rx) = mpsc::channel::<Command>();

    // Cover-art decode pipeline: audio thread sends raw image bytes (or None),
    // the art thread decodes/dithers off the audio + UI paths.
    let (art_tx, art_rx) = mpsc::channel::<Option<Vec<u8>>>();
    {
        let app = Arc::clone(&app);
        std::thread::Builder::new()
            .name("art".into())
            .stack_size(32 * 1024)
            .spawn(move || {
                while let Ok(bytes) = art_rx.recv() {
                    let bmp = bytes.and_then(|b| art::decode(&b));
                    let mut a = app.lock().unwrap();
                    a.art = bmp;
                    a.dirty = true;
                }
            })?;
    }

    {
        let app = Arc::clone(&app);
        let art_tx = art_tx.clone();
        std::thread::Builder::new()
            .name("audio".into())
            .stack_size(64 * 1024) // symphonia MP3 decode overflows a smaller stack -> reboot
            .spawn(move || audio_loop(app, audio_sink, rx, art_tx))?;
    }

    log::info!("main loop, {} tracks", app.lock().unwrap().queue.len());
    let mut tick: u32 = 0;
    loop {
        while let Some(btn) = ffi::buttons::poll() {
            log::info!("button {btn:?}");
            let cmd = app.lock().unwrap().on_button(btn);
            if cmd != Command::None {
                let _ = tx.send(cmd);
            }
        }

        tick = tick.wrapping_add(1);
        // Periodic progress update: push ONLY the bottom strip, so the colour
        // album art above isn't re-flushed (that caused the flashing).
        let mut progress_due = false;
        if tick % REPAINT_TICKS == 0 && app.lock().unwrap().state == PlaybackState::Playing {
            progress_due = true;
        }

        // Sample the battery every ~5s; repaint if the reading moved enough.
        if tick % 100 == 0 {
            let mv = ffi::battery::read_mv().unwrap_or(-1);
            let mut a = app.lock().unwrap();
            if (a.battery_mv - mv).abs() > 50 {
                a.dirty = true;
            }
            a.battery_mv = mv;
        }

        {
            let mut a = app.lock().unwrap();
            if a.dirty {
                display::mini_player::render(display.frame(), &a);
                display.flush();
                // Overlay the colour cover on top of the flushed 1bpp frame.
                #[cfg(feature = "ili9341")]
                if a.screen == app::Screen::NowPlaying {
                    if let Some(art) = &a.art {
                        ffi::ili9341::blit_rgb565(
                            display::mini_player::ART_X as u16,
                            display::mini_player::ART_Y as u16,
                            art.px as u16,
                            art.px as u16,
                            &art.data,
                        );
                    }
                }
                a.dirty = false;
            } else if progress_due {
                use display::framebuffer::{LCD_H, LCD_W};
                display::mini_player::render(display.frame(), &a);
                const BAND: i32 = 26;
                display.flush_rect(0, LCD_H as i32 - BAND, LCD_W as i32, BAND);
            }
        }

        std::thread::sleep(Duration::from_millis(50));
    }
}

/// Decode the current track and stream it to the speaker until end-of-track,
/// pause, or skip. Reacts to commands from the main thread.
fn audio_loop(
    app: Arc<Mutex<App>>,
    sink: audio::Audio,
    rx: mpsc::Receiver<Command>,
    art_tx: mpsc::Sender<Option<Vec<u8>>>,
) {
    let mut current: Option<Box<dyn decoder::Decoder>> = None;
    let mut pcm = [0i16; decoder::BLOCK_FRAMES * audio::CHANNELS as usize];

    loop {
        match rx.try_recv() {
            Ok(Command::LoadCurrent) | Ok(Command::Play) => {
                current = load_current(&app, &art_tx);
            }
            Ok(Command::Pause) => {
                sink.pause();
            }
            Ok(Command::None) | Err(mpsc::TryRecvError::Empty) => {}
            Err(mpsc::TryRecvError::Disconnected) => return,
        }

        let playing = app.lock().unwrap().state == PlaybackState::Playing;
        if !playing {
            std::thread::sleep(Duration::from_millis(20));
            continue;
        }

        let Some(dec) = current.as_mut() else {
            std::thread::sleep(Duration::from_millis(20));
            continue;
        };

        match dec.decode_into(&mut pcm) {
            Ok(0) => {
                let cmd = app.lock().unwrap().on_track_end();
                if cmd == Command::LoadCurrent {
                    current = load_current(&app, &art_tx);
                }
            }
            Ok(n) => {
                sink.play_block(&pcm[..n]);
                // Use the track's real rate/channels so the clock matches the
                // audio (tracks aren't always 44.1k stereo).
                let info = dec.info();
                let ch = (info.channels as usize).max(1);
                let sr = info.sample_rate.max(1);
                let frames = n / ch;
                let mut a = app.lock().unwrap();
                a.position += Duration::from_secs_f32(frames as f32 / sr as f32);
            }
            Err(e) => {
                log::error!("decode error: {e}");
                current = None;
            }
        }
    }
}

/// Open a decoder for the app's current track, stamping its real duration back
/// into the queue entry.
fn load_current(
    app: &Arc<Mutex<App>>,
    art_tx: &mpsc::Sender<Option<Vec<u8>>>,
) -> Option<Box<dyn decoder::Decoder>> {
    let path = {
        let a = app.lock().unwrap();
        a.current()?.path.clone()
    };
    match decoder::open(&path) {
        Ok(mut dec) => {
            let info = dec.info();
            ffi::audio_out::init(info.sample_rate, info.channels);
            let cover = dec.cover();
            {
                let mut a = app.lock().unwrap();
                let idx = a.index;
                if let Some(t) = a.queue.get_mut(idx) {
                    t.duration = info.duration;
                }
                a.position = Duration::ZERO;
                a.art = None; // clear old art until the new one decodes
                a.dirty = true;
            }
            let _ = art_tx.send(cover); // decode off-thread
            log::info!(
                "playing {} ({} Hz, {} ch)",
                path,
                info.sample_rate,
                info.channels
            );
            Some(dec)
        }
        Err(e) => {
            log::error!("open {path}: {e}");
            None
        }
    }
}
