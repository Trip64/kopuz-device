//! High-level display layer. Owns the framebuffer, renders the UI with
//! embedded-graphics, and pushes finished frames to the C panel driver.
//!
//! Two backends, chosen at compile time:
//!   * default          — Waveshare 2.13" e-paper (full/partial refresh).
//!   * `--features ili9341` — ILI9341 320x240 SPI TFT (full frame each flush).

pub mod framebuffer;
pub mod mini_player;

use framebuffer::FrameBuffer;

pub struct Display {
    fb: FrameBuffer,
    #[cfg(not(feature = "ili9341"))]
    full_refreshes: u32,
}

#[cfg(not(feature = "ili9341"))]
impl Display {
    pub fn new() -> Self {
        use crate::ffi::epd;
        epd::init();
        epd::clear();
        Self {
            fb: FrameBuffer::new(),
            full_refreshes: 0,
        }
    }

    pub fn frame(&mut self) -> &mut FrameBuffer {
        &mut self.fb
    }

    /// Full (clean) refresh every N partials to clear e-paper ghosting,
    /// partial otherwise for speed.
    pub fn flush(&mut self) {
        use crate::ffi::epd;
        const FULL_EVERY: u32 = 10;
        if self.full_refreshes % FULL_EVERY == 0 {
            epd::display_frame(self.fb.as_bytes());
        } else {
            epd::display_frame_partial(self.fb.as_bytes());
        }
        self.full_refreshes = self.full_refreshes.wrapping_add(1);
    }

    pub fn sleep(&mut self) {
        crate::ffi::epd::sleep();
    }
}

#[cfg(feature = "ili9341")]
impl Display {
    pub fn new() -> Self {
        use crate::ffi::ili9341;
        ili9341::init();
        ili9341::clear();
        Self {
            fb: FrameBuffer::new(),
        }
    }

    pub fn frame(&mut self) -> &mut FrameBuffer {
        &mut self.fb
    }

    /// TFT is fast: just push the whole frame each time.
    pub fn flush(&mut self) {
        crate::ffi::ili9341::display_frame(self.fb.as_bytes());
    }

    pub fn sleep(&mut self) {}
}

impl Default for Display {
    fn default() -> Self {
        Self::new()
    }
}
