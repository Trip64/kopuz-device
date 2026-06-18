#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ILI_WIDTH 320
#define ILI_HEIGHT 240
#define ILI_ROW_BYTES 40
#define ILI_FRAME_BYTES (ILI_ROW_BYTES * ILI_HEIGHT)

/// Bring up the panel (SPI + reset + init sequence + backlight on).
int ili9341_init(void);

/// Fill the whole panel with the background colour.
void ili9341_clear(void);

/// Set backlight brightness, 0..100 % (PWM on the BL pin).
void ili9341_set_brightness(uint8_t pct);

/// Blit an RGB565 (big-endian, 2 bytes/px) rectangle straight to the panel.
/// Used to draw colour album art over the 1bpp UI.
void ili9341_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                         const uint8_t *data);

/// Push only a sub-rectangle of the 1bpp frame `mono` (full-frame buffer) to
/// the panel, using the current theme colours.
void ili9341_display_region(const uint8_t *mono, uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h);

/// Set the 1bpp UI ink/background colours (RGB565). Album art stays true colour.
void ili9341_set_theme(uint16_t fg, uint16_t bg);

/// Push a 1bpp landscape framebuffer (ILI_ROW_BYTES per row, MSB first,
/// bit 1 = background/white) to the panel, expanding each bit to RGB565.
void ili9341_display_frame(const uint8_t *mono);

#ifdef __cplusplus
}
#endif
