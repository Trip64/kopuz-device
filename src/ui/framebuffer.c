#include "framebuffer.h"
#include <string.h>
#include <stdlib.h>

void fb_init(framebuffer_t *fb, uint8_t *buf, uint16_t w, uint16_t h) {
    fb->width = w;
    fb->height = h;
    fb->row_bytes = (w + 7) / 8;
    fb->buffer = buf;
    fb_clear(fb);
}

void fb_clear(framebuffer_t *fb) {
    if (!fb || !fb->buffer) return;
    memset(fb->buffer, 0xFF, (size_t)fb->row_bytes * fb->height);
}

void fb_fill(framebuffer_t *fb, bool on) {
    if (!fb || !fb->buffer) return;
    memset(fb->buffer, on ? 0x00 : 0xFF, (size_t)fb->row_bytes * fb->height);
}

void fb_set_pixel(framebuffer_t *fb, int16_t x, int16_t y, bool on) {
    if (!fb || !fb->buffer) return;
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) return;

    size_t idx = (size_t)y * fb->row_bytes + ((size_t)x >> 3);
    uint8_t mask = 0x80 >> (x & 7);

    if (on) {
        fb->buffer[idx] &= ~mask; // 0 = ink (black)
    } else {
        fb->buffer[idx] |= mask;  // 1 = background (white)
    }
}

bool fb_get_pixel(const framebuffer_t *fb, int16_t x, int16_t y) {
    if (!fb || !fb->buffer) return false;
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) return false;

    size_t idx = (size_t)y * fb->row_bytes + ((size_t)x >> 3);
    uint8_t mask = 0x80 >> (x & 7);
    return (fb->buffer[idx] & mask) == 0;
}

void fb_draw_line(framebuffer_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = -abs(y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;

    while (1) {
        fb_set_pixel(fb, x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void fb_draw_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
    if (w <= 0 || h <= 0) return;
    fb_draw_line(fb, x, y, x + w - 1, y, on);
    fb_draw_line(fb, x, y + h - 1, x + w - 1, y + h - 1, on);
    fb_draw_line(fb, x, y, x, y + h - 1, on);
    fb_draw_line(fb, x + w - 1, y, x + w - 1, y + h - 1, on);
}

void fb_fill_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
    if (!fb || w <= 0 || h <= 0) return;
    int16_t x1 = x + w;
    int16_t y1 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > fb->width) x1 = fb->width;
    if (y1 > fb->height) y1 = fb->height;

    for (int16_t py = y; py < y1; py++) {
        for (int16_t px = x; px < x1; px++) {
            fb_set_pixel(fb, px, py, on);
        }
    }
}

void fb_invert_rect(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (!fb || !fb->buffer || w <= 0 || h <= 0) return;
    int16_t x1 = x + w;
    int16_t y1 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > fb->width) x1 = fb->width;
    if (y1 > fb->height) y1 = fb->height;

    for (int16_t py = y; py < y1; py++) {
        for (int16_t px = x; px < x1; px++) {
            size_t idx = (size_t)py * fb->row_bytes + ((size_t)px >> 3);
            uint8_t mask = 0x80 >> (px & 7);
            fb->buffer[idx] ^= mask;
        }
    }
}

int16_t fb_draw_char(framebuffer_t *fb, int16_t x, int16_t y, char c, const font_t *font, bool inv) {
    if (!fb || !font) return 0;
    if (c < font->first_char || c > font->last_char) c = ' ';

    size_t char_idx = (size_t)(c - font->first_char);
    const uint8_t *glyph_data = &font->data[char_idx * font->height];

    for (uint8_t row = 0; row < font->height; row++) {
        uint8_t bits = glyph_data[row];
        for (uint8_t col = 0; col < font->width; col++) {
            bool bit = (bits & (0x80 >> col)) != 0;
            if (bit) {
                fb_set_pixel(fb, x + col, y + row, !inv);
            }
        }
    }
    return font->width;
}

int16_t fb_draw_text(framebuffer_t *fb, int16_t x, int16_t y, const char *str, const font_t *font, bool inv) {
    if (!fb || !str || !font) return 0;
    int16_t cur_x = x;
    while (*str) {
        cur_x += fb_draw_char(fb, cur_x, y, *str, font, inv);
        str++;
    }
    return cur_x - x;
}

int16_t fb_draw_text_trunc(framebuffer_t *fb, int16_t x, int16_t y, const char *str, size_t max_chars, const font_t *font, bool inv) {
    if (!fb || !str || !font) return 0;
    int16_t cur_x = x;
    size_t count = 0;
    while (*str && count < max_chars) {
        cur_x += fb_draw_char(fb, cur_x, y, *str, font, inv);
        str++;
        count++;
    }
    return cur_x - x;
}

void fb_blit_1bpp(framebuffer_t *fb, int16_t x0, int16_t y0, uint16_t w, uint16_t h, const uint8_t *data) {
    if (!fb || !data) return;
    uint16_t rb = (w + 7) / 8;

    for (uint16_t yy = 0; yy < h; yy++) {
        for (uint16_t xx = 0; xx < w; xx++) {
            uint8_t byte = data[yy * rb + (xx >> 3)];
            bool white = ((byte >> (7 - (xx & 7))) & 1) == 1;
            fb_set_pixel(fb, x0 + xx, y0 + yy, !white);
        }
    }
}
