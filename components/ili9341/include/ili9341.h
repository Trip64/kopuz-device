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

/// Push a 1bpp landscape framebuffer (ILI_ROW_BYTES per row, MSB first,
/// bit 1 = background/white) to the panel, expanding each bit to RGB565.
void ili9341_display_frame(const uint8_t *mono);

#ifdef __cplusplus
}
#endif
