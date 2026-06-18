//! Safe-ish Rust wrappers over the C components.
//!
//! The raw `extern "C"` symbols come from the C components via
//! `esp-idf-sys`'s bindgen pass (`bindings_header` in Cargo.toml). We re-expose
//! them here behind small functions so the rest of the firmware never writes
//! `unsafe` inline.

use esp_idf_sys as sys;

pub const EPD_WIDTH: usize = sys::EPD_WIDTH as usize;
pub const EPD_HEIGHT: usize = sys::EPD_HEIGHT as usize;
pub const EPD_FRAME_BYTES: usize = sys::EPD_FRAME_BYTES as usize;

pub const ILI_WIDTH: usize = sys::ILI_WIDTH as usize;
pub const ILI_HEIGHT: usize = sys::ILI_HEIGHT as usize;
pub const ILI_FRAME_BYTES: usize = sys::ILI_FRAME_BYTES as usize;

/// Buttons mirror of the C `btn_event_t` enum.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Button {
    PlayPause,
    Next,
    Prev,
    VolUp,
    VolDown,
    Back,
}

pub mod epd {
    use super::*;

    pub fn init() {
        unsafe { sys::epd_init() };
    }
    pub fn clear() {
        unsafe { sys::epd_clear() };
    }
    pub fn display_frame(buf: &[u8; EPD_FRAME_BYTES]) {
        unsafe { sys::epd_display_frame(buf.as_ptr()) };
    }
    pub fn display_frame_partial(buf: &[u8; EPD_FRAME_BYTES]) {
        unsafe { sys::epd_display_frame_partial(buf.as_ptr()) };
    }
    pub fn sleep() {
        unsafe { sys::epd_sleep() };
    }
}

pub mod ili9341 {
    use super::*;

    pub fn init() {
        unsafe { sys::ili9341_init() };
    }
    pub fn clear() {
        unsafe { sys::ili9341_clear() };
    }
    pub fn display_frame(buf: &[u8; ILI_FRAME_BYTES]) {
        unsafe { sys::ili9341_display_frame(buf.as_ptr()) };
    }
}

pub mod buttons {
    use super::*;

    pub fn init() {
        unsafe { sys::buttons_init() };
    }

    /// Non-blocking poll for the next debounced press.
    pub fn poll() -> Option<Button> {
        let ev = unsafe { sys::buttons_poll() };
        match ev {
            sys::btn_event_t_BTN_PLAY_PAUSE => Some(Button::PlayPause),
            sys::btn_event_t_BTN_NEXT => Some(Button::Next),
            sys::btn_event_t_BTN_PREV => Some(Button::Prev),
            sys::btn_event_t_BTN_VOL_UP => Some(Button::VolUp),
            sys::btn_event_t_BTN_VOL_DOWN => Some(Button::VolDown),
            sys::btn_event_t_BTN_BACK => Some(Button::Back),
            _ => None,
        }
    }
}

pub mod audio_out {
    use super::*;

    pub fn init(sample_rate: u32, channels: u8) {
        unsafe { sys::audio_out_init(sample_rate, channels) };
    }
    /// Write interleaved S16 frames; returns frames accepted.
    pub fn write(samples: &[i16], channels: u8) -> usize {
        let frames = samples.len() / channels as usize;
        unsafe { sys::audio_out_write(samples.as_ptr(), frames) }
    }
    pub fn set_volume(vol: u8) {
        unsafe { sys::audio_out_set_volume(vol) };
    }
    pub fn stop() {
        unsafe { sys::audio_out_stop() };
    }
}
