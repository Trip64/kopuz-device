#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "font.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t row_bytes;
    uint8_t *buffer; // 1bpp: bit 1 = background (white/theme bg), bit 0 = ink (black/theme fg)
} framebuffer_t;

void fb_init(framebuffer_t *fb, uint8_t *buf, uint16_t w, uint16_t h);
void fb_clear(framebuffer_t *fb);
void fb_fill(framebuffer_t *fb, bool on);
void fb_set_pixel(framebuffer_t *fb, int16_t x, int16_t y, bool on);
bool fb_get_pixel(const framebuffer_t *fb, int16_t x, int16_t y);

void fb_draw_line(framebuffer_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on);
void fb_draw_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on);
void fb_fill_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on);
void fb_invert_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h);

int16_t fb_draw_char(framebuffer_t *fb, int16_t x, int16_t y, char c, const font_t *font, bool inv);
int16_t fb_draw_text(framebuffer_t *fb, int16_t x, int16_t y, const char *str, const font_t *font, bool inv);
int16_t fb_draw_text_trunc(framebuffer_t *fb, int16_t x, int16_t y, const char *str, size_t max_chars, const font_t *font, bool inv);

void fb_blit_1bpp(framebuffer_t *fb, int16_t x0, int16_t y0, uint16_t w, uint16_t h, const uint8_t *data);

#ifdef __cplusplus
}
#endif
