//! 1bpp framebuffer, shared by both display backends.
//!
//! The UI is always drawn in landscape at `LCD_W` x `LCD_H`. The physical
//! layout differs per panel:
//!   * e-paper (default) — native RAM is portrait 122x250, so `set_pixel`
//!     rotates landscape coords 90° into it.
//!   * ILI9341 (`--features ili9341`) — native landscape 320x240, no rotation.
//! `bit 1 = background/white` in both, matching the C blit code.

use embedded_graphics::{pixelcolor::BinaryColor, prelude::*, primitives::Rectangle};

#[cfg(not(feature = "ili9341"))]
mod backend {
    use crate::ffi::{EPD_FRAME_BYTES, EPD_HEIGHT, EPD_WIDTH};
    pub const ROW_BYTES: usize = (EPD_WIDTH + 7) / 8;
    pub const FRAME_BYTES: usize = EPD_FRAME_BYTES;
    pub const LCD_W: usize = EPD_HEIGHT; // 250
    pub const LCD_H: usize = EPD_WIDTH; //  122

    /// Landscape (lx, ly) -> physical portrait byte/bit, 90° rotation.
    #[inline]
    pub fn index(lx: usize, ly: usize) -> (usize, u8) {
        let px = ly;
        let py = (LCD_W - 1) - lx;
        (py * ROW_BYTES + (px >> 3), 0x80u8 >> (px & 7))
    }
}

#[cfg(feature = "ili9341")]
mod backend {
    use crate::ffi::{ILI_FRAME_BYTES, ILI_HEIGHT, ILI_WIDTH};
    pub const ROW_BYTES: usize = (ILI_WIDTH + 7) / 8;
    pub const FRAME_BYTES: usize = ILI_FRAME_BYTES;
    pub const LCD_W: usize = ILI_WIDTH; //  320
    pub const LCD_H: usize = ILI_HEIGHT; // 240

    /// Native landscape, no rotation.
    #[inline]
    pub fn index(lx: usize, ly: usize) -> (usize, u8) {
        (ly * ROW_BYTES + (lx >> 3), 0x80u8 >> (lx & 7))
    }
}

pub use backend::{FRAME_BYTES, LCD_H, LCD_W};

pub struct FrameBuffer {
    buf: [u8; FRAME_BYTES],
}

impl FrameBuffer {
    pub fn new() -> Self {
        Self {
            buf: [0xFF; FRAME_BYTES],
        }
    }

    pub fn clear_white(&mut self) {
        self.buf.fill(0xFF);
    }

    /// All pixels black — panel data-path self-test.
    pub fn fill_black(&mut self) {
        self.buf.fill(0x00);
    }

    pub fn as_bytes(&self) -> &[u8; FRAME_BYTES] {
        &self.buf
    }

    #[inline]
    fn set_pixel(&mut self, lx: usize, ly: usize, on: bool) {
        if lx >= LCD_W || ly >= LCD_H {
            return;
        }
        let (idx, mask) = backend::index(lx, ly);
        if on {
            self.buf[idx] &= !mask;
        } else {
            self.buf[idx] |= mask;
        }
    }
}

impl Default for FrameBuffer {
    fn default() -> Self {
        Self::new()
    }
}

impl Dimensions for FrameBuffer {
    fn bounding_box(&self) -> Rectangle {
        Rectangle::new(Point::zero(), Size::new(LCD_W as u32, LCD_H as u32))
    }
}

impl DrawTarget for FrameBuffer {
    type Color = BinaryColor;
    type Error = core::convert::Infallible;

    fn draw_iter<I>(&mut self, pixels: I) -> Result<(), Self::Error>
    where
        I: IntoIterator<Item = Pixel<Self::Color>>,
    {
        for Pixel(coord, color) in pixels {
            if coord.x < 0 || coord.y < 0 {
                continue;
            }
            self.set_pixel(coord.x as usize, coord.y as usize, color.is_on());
        }
        Ok(())
    }
}
