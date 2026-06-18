//! High-level display layer. Owns the framebuffer, renders the UI with
//! embedded-graphics, and pushes finished frames to the C panel driver.

pub mod framebuffer;
pub mod mini_player;

use crate::ffi::epd;
use framebuffer::FrameBuffer;

pub struct Display {
    fb: FrameBuffer,
    full_refreshes: u32,
}

impl Display {
    /// Brings up the panel (C side) and clears it.
    pub fn new() -> Self {
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

    /// Flush the framebuffer. Does a full (clean) refresh every N partials to
    /// clear e-paper ghosting, partial otherwise for speed.
    pub fn flush(&mut self) {
        const FULL_EVERY: u32 = 10;
        if self.full_refreshes % FULL_EVERY == 0 {
            epd::display_frame(self.fb.as_bytes());
        } else {
            epd::display_frame_partial(self.fb.as_bytes());
        }
        self.full_refreshes = self.full_refreshes.wrapping_add(1);
    }

    pub fn sleep(&mut self) {
        epd::sleep();
    }
}

impl Default for Display {
    fn default() -> Self {
        Self::new()
    }
}
