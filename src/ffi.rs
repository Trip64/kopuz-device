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
    pub fn set_brightness(pct: u8) {
        unsafe { sys::ili9341_set_brightness(pct) };
    }
    /// Blit an RGB565 (big-endian) rectangle to the panel.
    pub fn blit_rgb565(x: u16, y: u16, w: u16, h: u16, data: &[u8]) {
        unsafe { sys::ili9341_blit_rgb565(x, y, w, h, data.as_ptr()) };
    }
    /// Push only a sub-rectangle of the 1bpp frame (used for the per-second
    /// progress update, so the colour art on top isn't flushed/flickered).
    pub fn display_region(buf: &[u8; ILI_FRAME_BYTES], x: u16, y: u16, w: u16, h: u16) {
        unsafe { sys::ili9341_display_region(buf.as_ptr(), x, y, w, h) };
    }
    /// Set the 1bpp UI colours (ink / background) as RGB565. Theme switch.
    pub fn set_theme(fg: u16, bg: u16) {
        unsafe { sys::ili9341_set_theme(fg, bg) };
    }
}

pub mod battery {
    use super::*;

    pub fn init() {
        unsafe { sys::battery_init() };
    }
    /// Battery voltage in mV (after the divider), or `None` on read error.
    pub fn read_mv() -> Option<i32> {
        let mv = unsafe { sys::battery_read_mv() };
        if mv < 0 {
            None
        } else {
            Some(mv)
        }
    }
}

pub mod ldr {
    use super::*;

    pub fn init() {
        unsafe { sys::ldr_init() };
    }
    pub fn read_raw() -> Option<i32> {
        let raw = unsafe { sys::ldr_read_raw() };
        if raw < 0 {
            None
        } else {
            Some(raw)
        }
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
    /// Write interleaved S32 samples; channel count comes from `init`. Returns
    /// samples accepted.
    pub fn write(samples: &[i32]) -> usize {
        unsafe { sys::audio_out_write(samples.as_ptr(), samples.len()) }
    }
    pub fn set_volume(vol: u8) {
        unsafe { sys::audio_out_set_volume(vol) };
    }
    pub fn stop() {
        unsafe { sys::audio_out_stop() };
    }
}
