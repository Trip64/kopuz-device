//! 1bpp framebuffer for the 2.13" panel.
//!
//! The panel's native RAM is portrait: 122 wide x 250 tall, MSB-first,
//! 16 bytes/row, bit 1 = white — exactly what the C `epd_display_frame`
//! expects, so `as_bytes()` stays in that physical layout.
//!
//! The UI is drawn in LANDSCAPE (250 wide x 122 tall). `set_pixel` rotates
//! logical landscape coords into the physical portrait buffer, so all of
//! embedded-graphics just works at 250x122.

use crate::ffi::{EPD_FRAME_BYTES, EPD_HEIGHT, EPD_WIDTH};
use embedded_graphics::{pixelcolor::BinaryColor, prelude::*, primitives::Rectangle};

const PANEL_W: usize = EPD_WIDTH;
const PANEL_H: usize = EPD_HEIGHT;
const ROW_BYTES: usize = (PANEL_W + 7) / 8;

/// Logical landscape canvas.
pub const LCD_W: usize = PANEL_H;
pub const LCD_H: usize = PANEL_W;

pub struct FrameBuffer {
    buf: [u8; EPD_FRAME_BYTES],
}

impl FrameBuffer {
    pub fn new() -> Self {
        Self { buf: [0xFF; EPD_FRAME_BYTES] }
    }

    pub fn clear_white(&mut self) {
        self.buf.fill(0xFF);
    }

    /// All pixels black — panel data-path self-test.
    pub fn fill_black(&mut self) {
        self.buf.fill(0x00);
    }

    pub fn as_bytes(&self) -> &[u8; EPD_FRAME_BYTES] {
        &self.buf
    }

    /// Landscape (lx, ly) -> physical portrait pixel, 90° rotation.
    #[inline]
    fn set_pixel(&mut self, lx: usize, ly: usize, on: bool) {
        if lx >= LCD_W || ly >= LCD_H {
            return;
        }
        let px = ly;
        let py = (LCD_W - 1) - lx;
        let idx = py * ROW_BYTES + (px >> 3);
        let mask = 0x80u8 >> (px & 7);
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
