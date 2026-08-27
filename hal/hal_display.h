#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize display panel, GPIOs, backlight, SPI / parallel bus
int hal_display_init(void);

// Set backlight brightness (0..100 %)
void hal_display_set_brightness(uint8_t pct);

// Set 1bpp theme colors (RGB565 foreground/ink and background)
void hal_display_set_theme(uint16_t fg, uint16_t bg);

// Clear display with background color
void hal_display_clear(void);

// Push a 1bpp landscape framebuffer to the display, expanding each bit using current theme
void hal_display_flush(const uint8_t *mono_fb);

// Push a sub-rectangle of the 1bpp frame to the display
void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Blit a raw RGB565 rectangle straight to the panel (for color album art)
void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data);

// Present composed frame to display output (atomic commit)
void hal_display_present(void);

// Put display to low-power sleep
void hal_display_sleep(void);

#ifdef __cplusplus
}
#endif
