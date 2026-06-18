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

    /// e-paper has no cheap partial-region push, so just do a normal flush.
    pub fn flush_rect(&mut self, _x: i32, _y: i32, _w: i32, _h: i32) {
        self.flush();
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

    /// Push only a sub-rectangle (per-second progress update) so the colour art
    /// elsewhere isn't re-flushed and flickered.
    pub fn flush_rect(&mut self, x: i32, y: i32, w: i32, h: i32) {
        crate::ffi::ili9341::display_region(
            self.fb.as_bytes(),
            x.max(0) as u16,
            y.max(0) as u16,
            w as u16,
            h as u16,
        );
    }

    pub fn sleep(&mut self) {}
}

impl Default for Display {
    fn default() -> Self {
        Self::new()
    }
}
