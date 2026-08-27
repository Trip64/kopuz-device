#include "hal/hal_display.h"
#include "config.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static uint16_t s_pixels[LCD_WIDTH * LCD_HEIGHT];

static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;
static uint8_t s_brightness = 100;

int hal_display_init(void) {
    if (s_window) return 0;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        printf("SDL_InitSubSystem VIDEO error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    int scale = 3;
    s_window = SDL_CreateWindow(
        "kopuz player",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LCD_WIDTH * scale,
        LCD_HEIGHT * scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!s_window) {
        printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_RaiseWindow(s_window);

    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s_renderer) {
        s_renderer = SDL_CreateRenderer(s_window, -1, 0);
    }

    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        LCD_WIDTH,
        LCD_HEIGHT
    );

    hal_display_clear();
    return 0;
}

void hal_display_set_brightness(uint8_t pct) {
    s_brightness = pct > 100 ? 100 : pct;
}

void hal_display_set_theme(uint16_t fg, uint16_t bg) {
    s_fg = fg;
    s_bg = bg;
}

void hal_display_clear(void) {
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        s_pixels[i] = s_bg;
    }
    hal_display_present();
}

void hal_display_flush(const uint8_t *mono_fb) {
    if (!mono_fb) return;

    uint16_t row_bytes = LCD_ROW_BYTES_1BPP;

    for (int y = 0; y < LCD_HEIGHT; y++) {
        const uint8_t *row = &mono_fb[y * row_bytes];
        for (int x = 0; x < LCD_WIDTH; x++) {
            int bit = (row[x >> 3] >> (7 - (x & 7))) & 1;
            s_pixels[y * LCD_WIDTH + x] = bit ? s_bg : s_fg;
        }
    }
}

void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!mono_fb || x + w > LCD_WIDTH || y + h > LCD_HEIGHT) return;

    uint16_t row_bytes = LCD_ROW_BYTES_1BPP;

    for (uint16_t ry = 0; ry < h; ry++) {
        uint16_t py = y + ry;
        const uint8_t *row = &mono_fb[py * row_bytes];
        for (uint16_t rx = 0; rx < w; rx++) {
            uint16_t px = x + rx;
            int bit = (row[px >> 3] >> (7 - (px & 7))) & 1;
            s_pixels[py * LCD_WIDTH + px] = bit ? s_bg : s_fg;
        }
    }
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data) {
    if (!rgb565_data || x + w > LCD_WIDTH || y + h > LCD_HEIGHT) return;

    for (uint16_t ry = 0; ry < h; ry++) {
        uint16_t py = y + ry;
        for (uint16_t rx = 0; rx < w; rx++) {
            uint16_t px = x + rx;
            size_t src_idx = ((size_t)ry * w + rx) * 2;
            uint16_t color = (uint16_t)((rgb565_data[src_idx] << 8) | rgb565_data[src_idx + 1]);
            s_pixels[py * LCD_WIDTH + px] = color;
        }
    }
}

void hal_display_present(void) {
    if (!s_texture || !s_renderer) return;

    SDL_Delay(4);

    uint32_t b_eff = 38 + ((uint32_t)s_brightness * (255 - 38)) / 100;
    SDL_SetTextureColorMod(s_texture, (uint8_t)b_eff, (uint8_t)b_eff, (uint8_t)b_eff);

    SDL_UpdateTexture(s_texture, NULL, s_pixels, LCD_WIDTH * sizeof(uint16_t));
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

void hal_display_sleep(void) {
}
