#include "hal/hal_display.h"
#include "sim_display.h"
#include "config.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

static const sim_display_profile_t S_PROFILES[DISP_PROFILE_COUNT] = {
    [DISP_PROFILE_STM32F7_480X272] = {
        .id = "f7",
        .name = "STM32F7 Mikromedia Plus",
        .desc = "480x272 Color TFT | 24-bit Parallel LTDC | 80x80 Album Art | Full RGB Themes",
        .width = 480,
        .height = 272,
        .scale = 2,
        .tech = DISP_TECH_COLOR_RGB565,
        .fixed_fg = 0xFFFF,
        .fixed_bg = 0x0000,
        .art_size = 80,
        .bus_delay_ms = 0
    },
    [DISP_PROFILE_ILI9341_320X240] = {
        .id = "ili",
        .name = "ILI9341 Color Display",
        .desc = "320x240 Color TFT | 40MHz SPI Bus | 80x80 Album Art | Full RGB Themes",
        .width = 320,
        .height = 240,
        .scale = 3,
        .tech = DISP_TECH_COLOR_RGB565,
        .fixed_fg = 0xFFFF,
        .fixed_bg = 0x0000,
        .art_size = 80,
        .bus_delay_ms = 2
    },
    [DISP_PROFILE_TDISPLAY_320X170] = {
        .id = "tdisplay",
        .name = "LilyGO T-Display S3",
        .desc = "320x170 Color AMOLED | Intel 8080 8-bit Parallel | 56x56 Album Art | Wide Color",
        .width = 320,
        .height = 170,
        .scale = 4,
        .tech = DISP_TECH_COLOR_RGB565,
        .fixed_fg = 0xFFFF,
        .fixed_bg = 0x0000,
        .fixed_accent = 0xFFFF,
        .art_size = 56,
        .bus_delay_ms = 1
    },
    [DISP_PROFILE_OLED_128X64] = {
        .id = "oled",
        .name = "SSD1306 Classic OLED",
        .desc = "128x64 Mono OLED | 400kHz I2C / SPI | Electric Blue Emissive | No Color / No Art",
        .width = 128,
        .height = 64,
        .scale = 8,
        .tech = DISP_TECH_OLED_BLUE_MONO,
        .fixed_fg = 0x3DEF, // Classic 0.96" OLED Electric Blue phosphor (R:0x07, G:0x3D, B:0x1F)
        .fixed_bg = 0x0000, // True Deep Emissive OLED Black
        .fixed_accent = 0x3DEF,
        .art_size = 0,
        .bus_delay_ms = 6
    },
    [DISP_PROFILE_OLED_128X128] = {
        .id = "oled128",
        .name = "SSD1327 Square OLED",
        .desc = "128x128 Square Mono OLED | 10MHz SPI | Pure White Emissive Phosphor | No Color / No Art",
        .width = 128,
        .height = 128,
        .scale = 6,
        .tech = DISP_TECH_OLED_WHITE_MONO,
        .fixed_fg = 0xFFFF, // Pure White OLED Phosphor
        .fixed_bg = 0x0000, // Pure Deep OLED Black
        .fixed_accent = 0xFFFF,
        .art_size = 0,
        .bus_delay_ms = 4
    },
    [DISP_PROFILE_SHARP_400X240] = {
        .id = "sharp",
        .name = "Sharp Memory LCD",
        .desc = "400x240 Reflective MIP | Ultra-Low Power 1bpp | Silver-White Paper Background | No Color",
        .width = 400,
        .height = 240,
        .scale = 3,
        .tech = DISP_TECH_SHARP_MIP_MONO,
        .fixed_fg = 0x0841, // High-Contrast Reflective Dark Charcoal / Black
        .fixed_bg = 0xDEFB, // Ambient Silver / Paper-White Reflective Background
        .fixed_accent = 0x0841,
        .art_size = 0,
        .bus_delay_ms = 3
    },
    [DISP_PROFILE_EPAPER_BWR_296X128] = {
        .id = "epaper",
        .name = "Waveshare BWR E-Paper",
        .desc = "296x128 Tri-Color E-Paper | Black/White/Red Pigments | Natural Paper Finish | Sunlight Readable",
        .width = 296,
        .height = 128,
        .scale = 4,
        .tech = DISP_TECH_EPAPER_BWR,
        .fixed_fg = 0x0000,     // Deep Black Ink
        .fixed_bg = 0xF7DE,     // Warm Natural Paper White
        .fixed_accent = 0xD800, // Vibrant E-Paper Crimson Red
        .art_size = 0,
        .bus_delay_ms = 180     // Simulated electrophoretic ink settling delay
    }
};

static uint8_t s_current_mode = DISP_PROFILE_STM32F7_480X272;
static uint8_t s_user_scale_override = 0;

static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static uint16_t s_pixels[MAX_LCD_WIDTH * MAX_LCD_HEIGHT];

static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;
static uint8_t s_brightness = 100;

static void get_effective_colors(uint16_t *fg, uint16_t *bg) {
    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    if (p->tech == DISP_TECH_COLOR_RGB565) {
        if (fg) *fg = s_fg;
        if (bg) *bg = s_bg;
    } else {
        // Enforce physical monochrome / E-Paper screen hardware palette
        if (fg) *fg = p->fixed_fg;
        if (bg) *bg = p->fixed_bg;
    }
}

void hal_sim_display_set_scale_override(uint8_t scale) {
    s_user_scale_override = scale;
}

uint8_t hal_sim_display_get_mode(void) {
    return s_current_mode;
}

uint8_t hal_sim_display_get_mode_count(void) {
    return DISP_PROFILE_COUNT;
}

const char* hal_sim_display_get_mode_name(uint8_t mode_idx) {
    if (mode_idx >= DISP_PROFILE_COUNT) mode_idx = 0;
    return S_PROFILES[mode_idx].name;
}

const char* hal_sim_display_get_mode_id(uint8_t mode_idx) {
    if (mode_idx >= DISP_PROFILE_COUNT) mode_idx = 0;
    return S_PROFILES[mode_idx].id;
}

const char* hal_sim_display_get_mode_desc(uint8_t mode_idx) {
    if (mode_idx >= DISP_PROFILE_COUNT) mode_idx = 0;
    return S_PROFILES[mode_idx].desc;
}

void hal_sim_display_get_size(uint16_t *w, uint16_t *h) {
    if (w) *w = S_PROFILES[s_current_mode].width;
    if (h) *h = S_PROFILES[s_current_mode].height;
}

bool hal_sim_display_is_color(void) {
    return (S_PROFILES[s_current_mode].tech == DISP_TECH_COLOR_RGB565);
}

bool hal_sim_display_supports_themes(void) {
    return (S_PROFILES[s_current_mode].tech == DISP_TECH_COLOR_RGB565);
}

uint8_t hal_sim_display_get_art_size(void) {
    return S_PROFILES[s_current_mode].art_size;
}

static void apply_window_profile(void) {
    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    uint8_t scale = s_user_scale_override ? s_user_scale_override : p->scale;

    char title[160];
    snprintf(title, sizeof(title), "kopuz player [%s - %ux%u @ %ux]", p->name, p->width, p->height, scale);

    if (s_window) {
        SDL_SetWindowTitle(s_window, title);
        SDL_SetWindowSize(s_window, p->width * scale, p->height * scale);
        SDL_SetWindowPosition(s_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    if (s_texture) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }

    if (s_renderer) {
        s_texture = SDL_CreateTexture(
            s_renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            p->width,
            p->height
        );
    }

    hal_display_clear();
}

void hal_sim_display_set_mode(uint8_t mode_idx) {
    if (mode_idx >= DISP_PROFILE_COUNT) mode_idx = 0;
    if (s_current_mode == mode_idx && s_texture != NULL) return;
    s_current_mode = mode_idx;
    if (s_window) {
        apply_window_profile();
    }
}

int hal_display_init(void) {
    if (s_window) return 0;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        printf("SDL_InitSubSystem VIDEO error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    uint8_t scale = s_user_scale_override ? s_user_scale_override : p->scale;

    char title[160];
    snprintf(title, sizeof(title), "kopuz player [%s - %ux%u @ %ux]", p->name, p->width, p->height, scale);

    s_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        p->width * scale,
        p->height * scale,
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

    apply_window_profile();
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
    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    uint16_t eff_fg, eff_bg;
    get_effective_colors(&eff_fg, &eff_bg);

    uint32_t total = (uint32_t)p->width * (uint32_t)p->height;
    for (uint32_t i = 0; i < total; i++) {
        s_pixels[i] = eff_bg;
    }
    hal_display_present();
}

void hal_display_flush(const uint8_t *mono_fb) {
    if (!mono_fb) return;

    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    uint16_t eff_fg, eff_bg;
    get_effective_colors(&eff_fg, &eff_bg);

    uint16_t row_bytes = (p->width + 7) / 8;

    for (int y = 0; y < p->height; y++) {
        const uint8_t *row = &mono_fb[y * row_bytes];
        for (int x = 0; x < p->width; x++) {
            int bit = (row[x >> 3] >> (7 - (x & 7))) & 1;
            uint16_t color = bit ? eff_bg : eff_fg;

            if (p->tech == DISP_TECH_EPAPER_BWR && !bit) {
                // In Tri-Color BWR E-Paper: Topbar header line & bottom progress bar / status accents render in vibrant Red pigment!
                if (y <= 13 || y >= p->height - 18) {
                    color = p->fixed_accent;
                }
            }

            s_pixels[y * p->width + x] = color;
        }
    }
}

void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!mono_fb) return;

    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    if (x + w > p->width || y + h > p->height) return;

    uint16_t eff_fg, eff_bg;
    get_effective_colors(&eff_fg, &eff_bg);

    uint16_t row_bytes = (p->width + 7) / 8;

    for (uint16_t ry = 0; ry < h; ry++) {
        uint16_t py = y + ry;
        const uint8_t *row = &mono_fb[py * row_bytes];
        for (uint16_t rx = 0; rx < w; rx++) {
            uint16_t px = x + rx;
            int bit = (row[px >> 3] >> (7 - (px & 7))) & 1;
            s_pixels[py * p->width + px] = bit ? eff_bg : eff_fg;
        }
    }
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data) {
    if (!rgb565_data) return;

    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    // Hardware limitation: Only full-color displays can render raw 16-bit RGB565 bitmaps
    if (p->tech != DISP_TECH_COLOR_RGB565 || x + w > p->width || y + h > p->height) return;

    for (uint16_t ry = 0; ry < h; ry++) {
        uint16_t py = y + ry;
        for (uint16_t rx = 0; rx < w; rx++) {
            uint16_t px = x + rx;
            size_t src_idx = ((size_t)ry * w + rx) * 2;
            uint16_t color = (uint16_t)((rgb565_data[src_idx] << 8) | rgb565_data[src_idx + 1]);
            s_pixels[py * p->width + px] = color;
        }
    }
}

void hal_sim_display_blit_art(int16_t dst_x, int16_t dst_y, uint8_t dst_w, uint8_t dst_h,
                             const uint8_t *src_rgb565, uint8_t src_w, uint8_t src_h) {
    if (!src_rgb565 || dst_w == 0 || dst_h == 0 || src_w == 0 || src_h == 0) return;

    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];
    if (p->tech != DISP_TECH_COLOR_RGB565) return;

    for (uint16_t dy = 0; dy < dst_h; dy++) {
        int16_t py = dst_y + (int16_t)dy;
        if (py < 0 || py >= p->height) continue;
        uint16_t sy = ((uint32_t)dy * (uint32_t)src_h) / (uint32_t)dst_h;

        for (uint16_t dx = 0; dx < dst_w; dx++) {
            int16_t px = dst_x + (int16_t)dx;
            if (px < 0 || px >= p->width) continue;
            uint16_t sx = ((uint32_t)dx * (uint32_t)src_w) / (uint32_t)dst_w;

            size_t src_idx = ((size_t)sy * src_w + sx) * 2;
            uint16_t color = (uint16_t)((src_rgb565[src_idx] << 8) | src_rgb565[src_idx + 1]);
            s_pixels[py * p->width + px] = color;
        }
    }
}

void hal_display_present(void) {
    if (!s_texture || !s_renderer) return;

    const sim_display_profile_t *p = &S_PROFILES[s_current_mode];

    if (p->bus_delay_ms > 0) {
        SDL_Delay(p->bus_delay_ms);
    }

    uint32_t b_eff = 38 + ((uint32_t)s_brightness * (255 - 38)) / 100;
    SDL_SetTextureColorMod(s_texture, (uint8_t)b_eff, (uint8_t)b_eff, (uint8_t)b_eff);

    SDL_UpdateTexture(s_texture, NULL, s_pixels, p->width * sizeof(uint16_t));
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

void hal_display_sleep(void) {
}
