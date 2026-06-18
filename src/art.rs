//! Album-art decoding. Takes an embedded cover JPEG, downscales it to the
//! panel's art box, and produces:
//!   * ILI9341 build  — RGB565 colour (blitted straight to the panel).
//!   * e-paper build  — 1bpp, Floyd–Steinberg dithered.
//! Runs on its own thread; progressive / oversized JPEGs are skipped because
//! they decode full-resolution and pin a CPU long enough to trip the watchdog.

use crate::display::framebuffer::ART_PX;
use std::io::Cursor;

/// Decoded album art ready to draw. `data` is RGB565 (2 bytes/px, big-endian)
/// on the ILI9341 build, or 1bpp ((px+7)/8 bytes/row, bit 1 = white) on e-paper.
pub struct ArtBmp {
    pub px: usize,
    pub data: Vec<u8>,
}

pub fn decode(bytes: &[u8]) -> Option<ArtBmp> {
    let mut dec = jpeg_decoder::Decoder::new(Cursor::new(bytes));
    dec.read_info().ok()?;
    let hdr = dec.info()?;
    // Progressive JPEGs decode full-res regardless of scaling and pin a core
    // for seconds (task watchdog); skip them. Also skip absurdly large images.
    if matches!(hdr.coding_process, jpeg_decoder::CodingProcess::DctProgressive) {
        return None;
    }
    if hdr.width as u32 * hdr.height as u32 > 4_000_000 {
        return None;
    }

    // Cap decode at 128px: full-res allocs MBs (OOM) and jpeg-decoder spawns
    // worker threads only when width > 128 (fails on this board).
    dec.scale(128, 128).ok()?;
    let pixels = dec.decode().ok()?;
    let info = dec.info()?;
    let (w, h) = (info.width as usize, info.height as usize);
    if w == 0 || h == 0 {
        return None;
    }
    let comp = match info.pixel_format {
        jpeg_decoder::PixelFormat::L8 => 1,
        jpeg_decoder::PixelFormat::RGB24 => 3,
        _ => return None,
    };

    let px = ART_PX;

    // Box-average each art pixel from its source region (keeps detail). Returns
    // (r, g, b) 0..255.
    let sample = |tx: usize, ty: usize| -> (u32, u32, u32) {
        let sy0 = ty * h / px;
        let sy1 = (((ty + 1) * h / px).max(sy0 + 1)).min(h);
        let sx0 = tx * w / px;
        let sx1 = (((tx + 1) * w / px).max(sx0 + 1)).min(w);
        let (mut r, mut g, mut b, mut n) = (0u32, 0u32, 0u32, 0u32);
        for sy in sy0..sy1 {
            for sx in sx0..sx1 {
                let i = (sy * w + sx) * comp;
                if comp == 1 {
                    let v = pixels[i] as u32;
                    r += v;
                    g += v;
                    b += v;
                } else {
                    r += pixels[i] as u32;
                    g += pixels[i + 1] as u32;
                    b += pixels[i + 2] as u32;
                }
                n += 1;
            }
        }
        let n = n.max(1);
        (r / n, g / n, b / n)
    };

    #[cfg(feature = "ili9341")]
    {
        let mut data = vec![0u8; px * px * 2];
        for ty in 0..px {
            for tx in 0..px {
                let (r, g, b) = sample(tx, ty);
                let v = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) as u16;
                let o = (ty * px + tx) * 2;
                data[o] = (v >> 8) as u8;
                data[o + 1] = (v & 0xFF) as u8;
            }
        }
        return Some(ArtBmp { px, data });
    }

    #[cfg(not(feature = "ili9341"))]
    {
        // Grayscale (Rec.601) then Floyd–Steinberg dither to 1bpp.
        let mut gray = vec![0i16; px * px];
        for ty in 0..px {
            for tx in 0..px {
                let (r, g, b) = sample(tx, ty);
                gray[ty * px + tx] = ((r * 77 + g * 150 + b * 29) >> 8) as i16;
            }
        }
        let rb = (px + 7) / 8;
        let mut data = vec![0xFFu8; rb * px];
        for y in 0..px {
            for x in 0..px {
                let idx = y * px + x;
                let old = gray[idx].clamp(0, 255);
                let newv: i16 = if old < 128 { 0 } else { 255 };
                let err = old - newv;
                if newv == 0 {
                    data[y * rb + (x >> 3)] &= !(0x80u8 >> (x & 7));
                }
                if x + 1 < px {
                    gray[idx + 1] += err * 7 / 16;
                }
                if y + 1 < px {
                    let below = idx + px;
                    if x > 0 {
                        gray[below - 1] += err * 3 / 16;
                    }
                    gray[below] += err * 5 / 16;
                    if x + 1 < px {
                        gray[below + 1] += err / 16;
                    }
                }
            }
        }
        return Some(ArtBmp { px, data });
    }
}
