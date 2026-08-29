#include "ui.h"
#include "font.h"
#include "framebuffer.h"
#include "hal/hal_system.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(TARGET_SIMULATOR)
bool hal_sim_display_is_color(void);
#endif

static void draw_battery(framebuffer_t *fb, const app_state_t *app);
static void draw_topbar(framebuffer_t *fb, const app_state_t *app);
static void draw_caret(framebuffer_t *fb, bool up);
static void draw_progress_bar(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, float frac);
static void render_list(framebuffer_t *fb, const app_state_t *app);
static void render_now_playing(framebuffer_t *fb, const app_state_t *app);
static void render_mini_footer(framebuffer_t *fb, const app_state_t *app);
static void draw_dithered_art_1bpp(framebuffer_t *fb, int16_t dst_x, int16_t dst_y, int16_t size,
                                   const uint8_t *src_rgb565, uint8_t src_size);

static const char* state_glyph(playback_state_t st) {
    switch (st) {
        case PLAYBACK_PLAYING: return ">";
        case PLAYBACK_PAUSED:  return "||";
        case PLAYBACK_STOPPED:
        default:               return "[]";
    }
}

static const char* repeat_str(repeat_mode_t r) {
    switch (r) {
        case REPEAT_ALL: return "All";
        case REPEAT_ONE: return "One";
        case REPEAT_OFF:
        default:         return "Off";
    }
}

static const char* repeat_short(repeat_mode_t r) {
    switch (r) {
        case REPEAT_ALL: return "A";
        case REPEAT_ONE: return "1";
        case REPEAT_OFF:
        default:         return "-";
    }
}

static void format_mmss(uint32_t secs, char *out, size_t out_len) {
    snprintf(out, out_len, "%u:%02u", (unsigned)(secs / 60), (unsigned)(secs % 60));
}

void ui_init(framebuffer_t *fb) {
    fb_clear(fb);
}

void ui_render(framebuffer_t *fb, const app_state_t *app) {
    if (!fb || !app) return;

    if (app->screen == SCREEN_BSOD) {
        ui_render_bsod(fb, app->stop_code, app->crash_details, NULL);
        return;
    }

    fb_clear(fb);

    if (app->screen == SCREEN_NOW_PLAYING) {
        render_now_playing(fb, app);
    } else {
        render_list(fb, app);
    }
}

void ui_render_message(framebuffer_t *fb, const char *heading, const char *body) {
    if (!fb) return;
    fb_clear(fb);

    fb_draw_text(fb, 6, 8, heading, &font_8x13_bold, false);
    fb_draw_line(fb, 0, 24, fb->width, 24, true);
    fb_draw_text(fb, 6, 32, body, &font_6x10, false);
}

static inline int16_t get_ui_header_y(const framebuffer_t *fb) {
    return (fb->height <= 64) ? 9 : ((fb->height <= 128) ? 12 : 14);
}

static inline int16_t get_ui_body_top(const framebuffer_t *fb) {
    return (fb->height <= 64) ? 11 : (get_ui_header_y(fb) + 2);
}

static inline int16_t get_ui_row_height(const framebuffer_t *fb) {
    return (fb->height <= 64) ? 10 : ((fb->height <= 128) ? 12 : 13);
}

static inline int16_t get_ui_footer_height(const framebuffer_t *fb) {
    return (fb->height <= 64) ? 12 : ((fb->height <= 128) ? 16 : 24);
}

static inline int16_t get_ui_footer_y(const framebuffer_t *fb) {
    return fb->height - get_ui_footer_height(fb);
}

static inline uint16_t get_ui_visible_rows(const framebuffer_t *fb) {
    int16_t h = get_ui_footer_y(fb) - get_ui_body_top(fb);
    int16_t r = get_ui_row_height(fb);
    return (h > 0 && r > 0) ? (uint16_t)(h / r) : 1;
}

static void draw_topbar(framebuffer_t *fb, const app_state_t *app) {
    char header[32] = {0};
    const char *title = "KOPUZ";

    switch (app->screen) {
        case SCREEN_MENU:         title = "KOPUZ"; break;
        case SCREEN_NOW_PLAYING:  title = "NOW PLAYING"; break;
        case SCREEN_SONGS:        title = "SONGS"; break;
        case SCREEN_ALBUMS:       title = "ALBUMS"; break;
        case SCREEN_ARTISTS:      title = "ARTISTS"; break;
        case SCREEN_SETTINGS:     title = "SETTINGS"; break;
        case SCREEN_ALBUM_TRACKS:
            if (app->open_group < app->albums_len) {
                title = app->albums[app->open_group].name;
            } else {
                title = "ALBUM";
            }
            break;
        case SCREEN_ARTIST_TRACKS:
            if (app->open_group < app->artists_len) {
                title = app->artists[app->open_group].name;
            } else {
                title = "ARTIST";
            }
            break;
        case SCREEN_BSOD:
            title = "CRASH";
            break;
    }

    size_t i = 0;
    while (title[i] && i < 22) {
        header[i] = (char)toupper((unsigned char)title[i]);
        i++;
    }
    header[i] = '\0';

    int16_t hy = get_ui_header_y(fb);
    int16_t ty = (fb->height <= 64) ? 0 : 1;
    fb_draw_text(fb, 2, ty, header, (fb->height <= 64) ? &font_6x10 : &font_8x13_bold, false);
    draw_battery(fb, app);
    fb_draw_line(fb, 0, hy, fb->width, hy, true);
}

static uint32_t get_ram_usage_bytes(const app_state_t *app) {
    uint32_t hw_ram = hal_system_get_ram_used_bytes();
    if (hw_ram > 0) {
        if (app && app->art_valid && app->art_rgb565) {
            hw_ram += (uint32_t)app->art_size * app->art_size * 2;
        }
        return hw_ram;
    }
    uint32_t ram = 25684;
    if (app) {
        if (app->art_valid && app->art_rgb565) {
            ram += (uint32_t)app->art_size * app->art_size * 2;
        }
        if (app->state == PLAYBACK_PLAYING) {
            ram += 3584;
        }
    }
    return ram;
}

static void draw_battery(framebuffer_t *fb, const app_state_t *app) {
    int8_t pct = app_get_battery_pct(app);
    char buf[16];

    if (pct < 0) {
        snprintf(buf, sizeof(buf), "USB");
    } else {
        snprintf(buf, sizeof(buf), "%d%%", pct);
    }

    int16_t tw = (int16_t)strlen(buf) * font_6x10.width;
    int16_t tx = fb->width - 2 - tw;
    int16_t ty = (fb->height <= 64) ? 0 : 1;
    fb_draw_text(fb, tx, ty, buf, &font_6x10, false);

    int16_t bat_left = tx;

    if (pct >= 0 && fb->width > 140) {
        int16_t bw = 14;
        int16_t bh = 7;
        int16_t iy = 2;
        int16_t ix = tx - bw - 4;
        bat_left = ix;

        fb_draw_rect(fb, ix, iy, bw, bh, true);
        fb_fill_rect(fb, ix + bw, iy + 2, 2, 3, true);

        int16_t fill = ((bw - 2) * pct) / 100;
        if (fill > 0) {
            fb_fill_rect(fb, ix + 1, iy + 1, fill, bh - 2, true);
        }
    }

    if (fb->width > 200) {
        uint32_t ram_bytes = get_ram_usage_bytes(app);
        uint32_t ram_kb = (ram_bytes + 1023) / 1024;
        char ram_str[16];
        snprintf(ram_str, sizeof(ram_str), "%uK", (unsigned)ram_kb);
        int16_t rw = (int16_t)strlen(ram_str) * font_6x10.width;
        int16_t rx = bat_left - rw - 6;
        fb_draw_text(fb, rx, ty, ram_str, &font_6x10, false);
    }
}

static void draw_caret(framebuffer_t *fb, bool up) {
    int16_t x = fb->width - 6;
    int16_t y = up ? get_ui_body_top(fb) : (get_ui_footer_y(fb) - 6);

    if (up) {
        fb_draw_line(fb, x, y + 4, x + 2, y, true);
        fb_draw_line(fb, x + 4, y + 4, x + 2, y, true);
    } else {
        fb_draw_line(fb, x, y, x + 2, y + 4, true);
        fb_draw_line(fb, x + 4, y, x + 2, y + 4, true);
    }
}

static void draw_progress_bar(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, float frac) {
    if (w <= 2) return;
    int16_t h = (fb->height <= 64) ? 4 : 5;
    fb_draw_rect(fb, x, y, w, h, true);

    if (frac > 1.0f) frac = 1.0f;
    if (frac < 0.0f) frac = 0.0f;

    int16_t fill = (int16_t)((w - 2) * frac);
    if (fill > 0) {
        fb_fill_rect(fb, x + 1, y + 1, fill, h - 2, true);
    }
}

static void draw_spectrum(framebuffer_t *fb, int16_t x, int16_t y, int16_t max_h, const app_state_t *app) {
    if (!fb || !app || !app->vu_enabled) return;
    int16_t bar_w = (fb->width <= 140) ? 4 : 5;
    int16_t gap = (fb->width <= 140) ? 1 : 2;

    for (int b = 0; b < 8; b++) {
        int16_t bx = x + b * (bar_w + gap);
        uint8_t lvl = (uint8_t)(((uint32_t)app->vu_meter[b] * (uint32_t)max_h) / 24);
        if (lvl > max_h) lvl = (uint8_t)max_h;

        fb_draw_line(fb, bx, y + max_h, bx + bar_w - 1, y + max_h, true);

        if (lvl > 0) {
            for (int16_t h = 2; h <= lvl; h += 3) {
                fb_fill_rect(fb, bx, y + (max_h - h), bar_w, 2, true);
            }
        }

        uint8_t pk = (uint8_t)(((uint32_t)app->vu_peak[b] * (uint32_t)max_h) / 24);
        if (pk > max_h) pk = (uint8_t)max_h;
        if (pk > 1) {
            fb_draw_line(fb, bx, y + (max_h - pk), bx + bar_w - 1, y + (max_h - pk), true);
        }
    }
}

static void draw_slider(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct, bool inv) {
    fb_draw_rect(fb, x, y, w, h, !inv);
    int16_t fill = (int16_t)(((w - 2) * pct) / 100);
    if (fill > 0) {
        fb_fill_rect(fb, x + 1, y + 1, fill, h - 2, !inv);
    }
}

static void render_list(framebuffer_t *fb, const app_state_t *app) {
    draw_topbar(fb, app);

    uint16_t n = app_get_list_len(app);
    uint16_t sel = app_get_current_selection(app);
    int16_t body_top = get_ui_body_top(fb);
    int16_t row_h = get_ui_row_height(fb);
    uint16_t visible = get_ui_visible_rows(fb);

    if (n == 0) {
        const char *empty_msg = "No tracks found on /sdcard.";
        if (app->screen == SCREEN_ALBUMS) empty_msg = "No albums found.";
        else if (app->screen == SCREEN_ARTISTS) empty_msg = "No artists found.";
        fb_draw_text(fb, 8, body_top + 8, empty_msg, &font_6x10, false);
    } else {
        uint16_t max_start = (n > visible) ? (n - visible) : 0;
        uint16_t start = (sel > visible / 2) ? (sel - visible / 2) : 0;
        if (start > max_start) start = max_start;

        for (uint16_t row = 0; row < visible; row++) {
            uint16_t idx = start + row;
            if (idx >= n) break;

            int16_t y = body_top + row * row_h;
            bool is_selected = (idx == sel);

            if (is_selected) {
                fb_fill_rect(fb, 2, y - 1, fb->width - 4, row_h, true);
            }

            char line[96] = {0};
            switch (app->screen) {
                case SCREEN_MENU: {
                    const char *icon = " ";
                    if (idx == 0) icon = ">";
                    else if (idx == 1) icon = "#";
                    else if (idx == 2) icon = "@";
                    else if (idx == 3) icon = "~";
                    else if (idx == 4) icon = "*";
                    snprintf(line, sizeof(line), "%s %s", icon, MENU_ITEMS[idx]);
                    break;
                }
                case SCREEN_SETTINGS:
                    if (fb->width <= 140) {
                        if (idx == 0) snprintf(line, sizeof(line), "Shuffle: %s", app->shuffle ? "ON" : "OFF");
                        else if (idx == 1) snprintf(line, sizeof(line), "Repeat:  %s", repeat_str(app->repeat));
                        else if (idx == 2) snprintf(line, sizeof(line), "Volume:  %u%%", app->volume);
                        else if (idx == 3) snprintf(line, sizeof(line), "Bright:  %u%%", app->brightness);
                        else if (idx == 4) snprintf(line, sizeof(line), "Theme:   Mono");
#if HAS_BLE_AUDIO
                        else if (idx == 5) snprintf(line, sizeof(line), "Output:  %s", (app->output_mode == OUTPUT_BLE_AUDIO) ? "BLE" : "I2S");
                        else if (idx == 6) snprintf(line, sizeof(line), "VU:      %s", app->vu_enabled ? "ON" : "OFF");
                        else if (idx == 7) snprintf(line, sizeof(line), "Save:    %s", (app->config_store == CONFIG_STORE_EEPROM) ? "EEPROM" : "SD");
#else
                        else if (idx == 5) snprintf(line, sizeof(line), "VU:      %s", app->vu_enabled ? "ON" : "OFF");
                        else if (idx == 6) snprintf(line, sizeof(line), "Save:    %s", (app->config_store == CONFIG_STORE_EEPROM) ? "EEPROM" : "SD");
#endif
                    } else {
                        if (idx == 0) snprintf(line, sizeof(line), "Shuffle:    [%s]", app->shuffle ? "ON" : "OFF");
                        else if (idx == 1) snprintf(line, sizeof(line), "Repeat:     [%s]", repeat_str(app->repeat));
                        else if (idx == 2) snprintf(line, sizeof(line), "Volume:        %u%%", app->volume);
                        else if (idx == 3) snprintf(line, sizeof(line), "Brightness:    %u%%", app->brightness);
                        else if (idx == 4) {
                            if (fb->width == 400 && fb->height == 240) {
                                snprintf(line, sizeof(line), "Theme:      [Reflective MIP]");
                            } else if (fb->width == 296 && fb->height == 128) {
                                snprintf(line, sizeof(line), "Theme:      [Tri-Color BWR]");
                            } else {
                                snprintf(line, sizeof(line), "Theme:      %s", THEMES[app->theme_index].name);
                            }
                        }
#if HAS_BLE_AUDIO
                        else if (idx == 5) snprintf(line, sizeof(line), "Output:     [%s]", (app->output_mode == OUTPUT_BLE_AUDIO) ? "BLE AUDIO" : "I2S DAC");
                        else if (idx == 6) snprintf(line, sizeof(line), "Visualizer: [%s]", app->vu_enabled ? "ON" : "OFF");
                        else if (idx == 7) snprintf(line, sizeof(line), "Storage:    [%s]", (app->config_store == CONFIG_STORE_EEPROM) ? "EEPROM (NVS)" : "SD CARD (.CFG)");
#else
                        else if (idx == 5) snprintf(line, sizeof(line), "Visualizer: [%s]", app->vu_enabled ? "ON" : "OFF");
                        else if (idx == 6) snprintf(line, sizeof(line), "Storage:    [%s]", (app->config_store == CONFIG_STORE_EEPROM) ? "EEPROM (NVS)" : "SD CARD (.CFG)");
#endif
                    }
                    break;
                case SCREEN_SONGS: {
                    const char *m = " ";
                    if (idx == app->current_index && app->state != PLAYBACK_STOPPED) {
                        m = state_glyph(app->state);
                    }
                    snprintf(line, sizeof(line), "%s %s", m, app->queue[idx].title);
                    break;
                }
                case SCREEN_ALBUMS:
                    if (idx < app->albums_len) {
                        snprintf(line, sizeof(line), "@ %s (%u)", app->albums[idx].name, app->albums[idx].count);
                    }
                    break;
                case SCREEN_ARTISTS:
                    if (idx < app->artists_len) {
                        snprintf(line, sizeof(line), "~ %s (%u)", app->artists[idx].name, app->artists[idx].count);
                    }
                    break;
                case SCREEN_ALBUM_TRACKS:
                case SCREEN_ARTIST_TRACKS: {
                    track_group_t *grp = (app->screen == SCREEN_ALBUM_TRACKS)
                        ? &app->albums[app->open_group]
                        : &app->artists[app->open_group];
                    if (idx < grp->count) {
                        uint16_t master = grp->track_indices[idx];
                        const char *m = " ";
                        if (master == app->current_index && app->state != PLAYBACK_STOPPED) {
                            m = state_glyph(app->state);
                        }
                        snprintf(line, sizeof(line), "%s %s", m, app->queue[master].title);
                    }
                    break;
                }
                default:
                    break;
            }

            if (fb->width >= 360 && (app->screen == SCREEN_SONGS || app->screen == SCREEN_ALBUM_TRACKS || app->screen == SCREEN_ARTIST_TRACKS)) {
                uint16_t master = idx;
                if (app->screen == SCREEN_ALBUM_TRACKS) {
                    master = app->albums[app->open_group].track_indices[idx];
                } else if (app->screen == SCREEN_ARTIST_TRACKS) {
                    master = app->artists[app->open_group].track_indices[idx];
                }

                const char *m = " ";
                if (master == app->current_index && app->state != PLAYBACK_STOPPED) {
                    m = state_glyph(app->state);
                }
                char title_buf[64];
                snprintf(title_buf, sizeof(title_buf), "%s %s", m, app->queue[master].title);
                size_t t_cap = (size_t)((fb->width - 170) / font_6x10.width);
                fb_draw_text_trunc(fb, 6, y + 1, title_buf, t_cap, &font_6x10, is_selected);

                fb_draw_text_trunc(fb, fb->width - 160, y + 1, app->queue[master].artist, 18, &font_6x10, is_selected);

                char dur_str[16];
                format_mmss(app->queue[master].duration_secs, dur_str, sizeof(dur_str));
                fb_draw_text(fb, fb->width - 44, y + 1, dur_str, &font_6x10, is_selected);
            } else {
                size_t max_chars = (size_t)((fb->width - 12) / font_6x10.width);
                fb_draw_text_trunc(fb, 6, y + 1, line, max_chars, &font_6x10, is_selected);
            }

            if (app->screen == SCREEN_SETTINGS && fb->width >= 200) {
                if (idx == 2) {
                    draw_slider(fb, fb->width - 70, y + 2, 45, 6, app->volume, is_selected);
                } else if (idx == 3) {
                    draw_slider(fb, fb->width - 70, y + 2, 45, 6, app->brightness, is_selected);
                }
            }
        }

        if (start > 0) draw_caret(fb, true);
        if (start + visible < n) draw_caret(fb, false);
    }

    render_mini_footer(fb, app);
}

static void draw_dithered_art_1bpp(framebuffer_t *fb, int16_t dst_x, int16_t dst_y, int16_t size,
                                   const uint8_t *src_rgb565, uint8_t src_size) {
    if (!fb || !src_rgb565 || size <= 0 || src_size == 0) return;

    static const uint8_t bayer4x4[4][4] = {
        {  0,  8,  2, 10 },
        { 12,  4, 14,  6 },
        {  3, 11,  1,  9 },
        { 15,  7, 13,  5 }
    };

    for (int16_t dy = 0; dy < size; dy++) {
        uint16_t sy = ((uint32_t)dy * (uint32_t)src_size) / (uint32_t)size;
        for (int16_t dx = 0; dx < size; dx++) {
            uint16_t sx = ((uint32_t)dx * (uint32_t)src_size) / (uint32_t)size;
            size_t idx = ((size_t)sy * src_size + sx) * 2;
            uint16_t c = (uint16_t)((src_rgb565[idx] << 8) | src_rgb565[idx + 1]);

            // RGB565 -> Luminance (0..255)
            uint8_t r = (uint8_t)(((c >> 11) & 0x1F) << 3);
            uint8_t g = (uint8_t)(((c >> 5) & 0x3F) << 2);
            uint8_t b = (uint8_t)((c & 0x1F) << 3);
            uint8_t lum = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);

            uint8_t threshold = (bayer4x4[dy & 3][dx & 3] * 16) + 8;
            bool ink = (lum < threshold);

            fb_set_pixel(fb, dst_x + dx, dst_y + dy, ink);
        }
    }
}

static void render_now_playing(framebuffer_t *fb, const app_state_t *app) {
    draw_topbar(fb, app);

    const track_t *cur = app_get_current_track(app);
    if (!cur) {
        fb_draw_text(fb, 4, get_ui_body_top(fb) + 4, "No track playing", &font_6x10, false);
        return;
    }

    if (fb->width <= 140 && fb->height <= 80) {
        // Compact 128x64 OLED Layout (Pixel-perfect 64px bounds)
        fb_draw_text_trunc(fb, 2, 10, cur->title, 20, &font_6x10, false);
        fb_draw_text_trunc(fb, 2, 20, cur->artist, 20, &font_6x10, false);

        draw_spectrum(fb, 2, 30, 8, app);

        char tags[32] = {0};
        if (app->shuffle) strcat(tags, "S ");
        if (app->repeat != REPEAT_OFF) strcat(tags, "R ");
        if (app->format_badge[0] != '\0') {
            strcat(tags, app->format_badge);
        }
        fb_draw_text_trunc(fb, 46, 30, tags, 13, &font_6x10, false);

        float frac = 0.0f;
        if (cur->duration_secs > 0) {
            frac = (float)(app->position_ms / 1000) / (float)cur->duration_secs;
        }
        draw_progress_bar(fb, 2, 40, fb->width - 4, frac);

        char tline[32];
        char pos_str[16], dur_str[16];
        format_mmss(app->position_ms / 1000, pos_str, sizeof(pos_str));
        format_mmss(cur->duration_secs, dur_str, sizeof(dur_str));
        snprintf(tline, sizeof(tline), "%s %s/%s", state_glyph(app->state), pos_str, dur_str);
        fb_draw_text(fb, 2, 48, tline, &font_6x10, false);

        char vol_str[16];
        snprintf(vol_str, sizeof(vol_str), "v%u", app->volume);
        int16_t vx = fb->width - (int16_t)strlen(vol_str) * font_6x10.width - 2;
        fb_draw_text(fb, vx, 48, vol_str, &font_6x10, false);
    } else if (fb->width <= 140 && fb->height > 80) {
        // Square 128x128 OLED Layout (Pixel-perfect 128px bounds)
        fb_draw_text_trunc(fb, 2, 14, cur->title, 20, &font_8x13_bold, false);
        fb_draw_text_trunc(fb, 2, 29, cur->artist, 20, &font_6x10, false);
        fb_draw_text_trunc(fb, 2, 40, cur->album, 20, &font_6x10, false);

        draw_spectrum(fb, 2, 53, 16, app);

        char tags[32] = {0};
        if (app->shuffle) strcat(tags, "SHUF ");
        if (app->repeat != REPEAT_OFF) {
            char rpt[16];
            snprintf(rpt, sizeof(rpt), "RPT:%s ", repeat_str(app->repeat));
            strcat(tags, rpt);
        }
        if (app->format_badge[0] != '\0') {
            strcat(tags, app->format_badge);
        }
        fb_draw_text(fb, 50, 53, tags, &font_6x10, false);

        // Upcoming queue hint at y=74..98
        uint16_t upcoming[2];
        size_t up_count = app_get_upcoming(app, upcoming, 2);
        if (up_count > 0) {
            for (size_t k = 0; k < up_count; k++) {
                uint16_t trk_idx = upcoming[k];
                if (trk_idx < app->queue_len) {
                    char next_str[64];
                    snprintf(next_str, sizeof(next_str), "> %s", app->queue[trk_idx].title);
                    fb_draw_text_trunc(fb, 2, 74 + (int16_t)k * 11, next_str, 20, &font_6x10, false);
                }
            }
        }

        float frac = 0.0f;
        if (cur->duration_secs > 0) {
            frac = (float)(app->position_ms / 1000) / (float)cur->duration_secs;
        }
        draw_progress_bar(fb, 2, 102, fb->width - 4, frac);

        char tline[32];
        char pos_str[16], dur_str[16];
        format_mmss(app->position_ms / 1000, pos_str, sizeof(pos_str));
        format_mmss(cur->duration_secs, dur_str, sizeof(dur_str));
        snprintf(tline, sizeof(tline), "%s %s/%s", state_glyph(app->state), pos_str, dur_str);
        fb_draw_text(fb, 2, 110, tline, &font_6x10, false);

        char vol_str[16];
        snprintf(vol_str, sizeof(vol_str), "v%u", app->volume);
        int16_t vx = fb->width - (int16_t)strlen(vol_str) * font_6x10.width - 2;
        fb_draw_text(fb, vx, 110, vol_str, &font_6x10, false);
    } else if (fb->width >= 360) {
        // Widescreen Pro Dashboard Layout (Sharp 400x240, STM32F7 480x272)
        int16_t ax = 6;
        int16_t ay = get_ui_body_top(fb) + 2;
        int16_t art_size = 80;

        if (app->art_valid && app->art_rgb565) {
            fb_draw_rect(fb, ax, ay, art_size, art_size, true);
#if defined(TARGET_SIMULATOR)
            if (!hal_sim_display_is_color()) {
                draw_dithered_art_1bpp(fb, ax + 1, ay + 1, art_size - 2, app->art_rgb565, app->art_size ? app->art_size : 80);
            }
#elif !COLOR_DISPLAY
            draw_dithered_art_1bpp(fb, ax + 1, ay + 1, art_size - 2, app->art_rgb565, app->art_size ? app->art_size : 80);
#endif
        } else {
            fb_draw_rect(fb, ax, ay, art_size, art_size, true);
            fb_draw_rect(fb, ax + 2, ay + 2, art_size - 4, art_size - 4, true);
            const char *g = state_glyph(app->state);
            int16_t gw = (int16_t)strlen(g) * font_8x13_bold.width;
            fb_draw_text(fb, ax + (art_size - gw) / 2, ay + (art_size - font_8x13_bold.height) / 2, g, &font_8x13_bold, false);
        }

        // Left Column: Spectrum Visualizer & Track Indicator below art
        int16_t by = fb->height - 24;
        int16_t vu_y = ay + art_size + 6;
        int16_t vu_max_h = by - vu_y - 18;
        if (vu_max_h > 46) vu_max_h = 46;
        if (vu_max_h >= 12) {
            draw_spectrum(fb, ax + 1, vu_y, vu_max_h, app);
        }

        char trk_idx_str[32];
        snprintf(trk_idx_str, sizeof(trk_idx_str), "TRK %u/%u", (unsigned)(app->current_index + 1), (unsigned)app->queue_len);
        fb_draw_text(fb, ax + 2, by - 12, trk_idx_str, &font_6x10, false);

        // Center Column: Track Metadata & Technical Specs
        int16_t cx = ax + art_size + 10;
        int16_t split_x = fb->width - 156;
        fb_draw_line(fb, split_x - 6, ay, split_x - 6, by - 2, true);

        size_t c_cap = (size_t)((split_x - cx - 10) / font_6x10.width);
        size_t c_bold_cap = (size_t)((split_x - cx - 10) / font_8x13_bold.width);

        fb_draw_text_trunc(fb, cx, ay, cur->title, c_bold_cap, &font_8x13_bold, false);
        fb_draw_text_trunc(fb, cx, ay + 15, cur->artist, c_cap, &font_6x10, false);
        fb_draw_text_trunc(fb, cx, ay + 27, cur->album, c_cap, &font_6x10, false);
        fb_draw_line(fb, cx, ay + 41, split_x - 12, ay + 41, true);

        char fmt_line[64] = {0};
        if (app->format_badge[0] != '\0') {
            snprintf(fmt_line, sizeof(fmt_line), "AUDIO: %s", app->format_badge);
        } else {
            snprintf(fmt_line, sizeof(fmt_line), "AUDIO: PCM 16b/44.1k");
        }
        fb_draw_text_trunc(fb, cx, ay + 47, fmt_line, c_cap, &font_6x10, false);

        char mode_line[64];
        snprintf(mode_line, sizeof(mode_line), "MODES: %s  %s", app->shuffle ? "SHUF" : "NORM", repeat_str(app->repeat));
        fb_draw_text_trunc(fb, cx, ay + 59, mode_line, c_cap, &font_6x10, false);

        char dsp_line[64];
        snprintf(dsp_line, sizeof(dsp_line), "VOL:   %u%%  EQ:Flat", app->volume);
        fb_draw_text_trunc(fb, cx, ay + 71, dsp_line, c_cap, &font_6x10, false);

        char buf_line[64];
        snprintf(buf_line, sizeof(buf_line), "BUF:   100%% (4KB)");
        fb_draw_text_trunc(fb, cx, ay + 83, buf_line, c_cap, &font_6x10, false);

        char st_line[64];
        snprintf(st_line, sizeof(st_line), "STATE: %s", (app->state == PLAYBACK_PLAYING) ? "PLAYING" : ((app->state == PLAYBACK_PAUSED) ? "PAUSED" : "STOPPED"));
        fb_draw_text_trunc(fb, cx, ay + 95, st_line, c_cap, &font_6x10, false);

        // Right Column: Dedicated Up Next In Queue
        int16_t rx = split_x;
        size_t r_cap = (size_t)((fb->width - rx - 4) / font_6x10.width);
        fb_draw_text(fb, rx, ay, "UP NEXT IN QUEUE:", &font_6x10, false);
        fb_draw_line(fb, rx, ay + 11, fb->width - 4, ay + 11, true);

        int16_t q_avail = by - 4 - (ay + 15);
        int16_t rows = q_avail / 12;
        if (rows > 8) rows = 8;

        if (rows > 0) {
            uint16_t upcoming[8];
            size_t up_count = app_get_upcoming(app, upcoming, (size_t)rows);
            for (size_t k = 0; k < up_count; k++) {
                uint16_t trk_idx = upcoming[k];
                if (trk_idx < app->queue_len) {
                    char row_str[96];
                    char d_str[16];
                    format_mmss(app->queue[trk_idx].duration_secs, d_str, sizeof(d_str));
                    snprintf(row_str, sizeof(row_str), "%u.%s", (unsigned)(k + 1), app->queue[trk_idx].title);
                    fb_draw_text_trunc(fb, rx, ay + 15 + (int16_t)k * 12, row_str, r_cap > 5 ? (r_cap - 5) : r_cap, &font_6x10, false);
                    fb_draw_text(fb, fb->width - 34, ay + 15 + (int16_t)k * 12, d_str, &font_6x10, false);
                }
            }
        }

        // Bottom Progress Bar & Time
        float frac = 0.0f;
        if (cur->duration_secs > 0) {
            frac = (float)(app->position_ms / 1000) / (float)cur->duration_secs;
        }
        draw_progress_bar(fb, 4, by, fb->width - 8, frac);

        char pos_str[16], dur_str[16], rem_str[16];
        format_mmss(app->position_ms / 1000, pos_str, sizeof(pos_str));
        format_mmss(cur->duration_secs, dur_str, sizeof(dur_str));
        uint32_t rem_sec = (cur->duration_secs > (app->position_ms / 1000)) ? (cur->duration_secs - (app->position_ms / 1000)) : 0;
        format_mmss(rem_sec, rem_str, sizeof(rem_str));

        fb_draw_text(fb, 4, by + 8, pos_str, &font_6x10, false);

        char mid_title[96];
        snprintf(mid_title, sizeof(mid_title), "%s %s", state_glyph(app->state), cur->title);
        int16_t mid_w = (int16_t)strlen(mid_title) * font_6x10.width;
        int16_t mid_x = (fb->width - mid_w) / 2;
        if (mid_x > 60 && mid_x + mid_w < fb->width - 70) {
            fb_draw_text(fb, mid_x, by + 8, mid_title, &font_6x10, false);
        }

        char tline_right[32];
        snprintf(tline_right, sizeof(tline_right), "-%s / %s", rem_str, dur_str);
        int16_t rx_t = fb->width - (int16_t)strlen(tline_right) * font_6x10.width - 4;
        fb_draw_text(fb, rx_t, by + 8, tline_right, &font_6x10, false);
    } else {
        // Wide / Full-Size Color & E-Ink Layout (320x170 T-Display, 320x240 ILI9341, 296x128 BWR E-Paper)
        int16_t ax = 4;
        int16_t ay = get_ui_body_top(fb) + 2;
        int16_t art_size = (fb->height <= 130) ? 48 : ((fb->height <= 180) ? 56 : 80);

        if (app->art_valid && app->art_rgb565) {
            fb_draw_rect(fb, ax, ay, art_size, art_size, true);
#if defined(TARGET_SIMULATOR)
            if (!hal_sim_display_is_color()) {
                draw_dithered_art_1bpp(fb, ax + 1, ay + 1, art_size - 2, app->art_rgb565, app->art_size ? app->art_size : 80);
            }
#elif !COLOR_DISPLAY
            draw_dithered_art_1bpp(fb, ax + 1, ay + 1, art_size - 2, app->art_rgb565, app->art_size ? app->art_size : 80);
#endif
        } else {
            fb_draw_rect(fb, ax, ay, art_size, art_size, true);
            fb_draw_rect(fb, ax + 2, ay + 2, art_size - 4, art_size - 4, true);
            const char *g = state_glyph(app->state);
            int16_t gw = (int16_t)strlen(g) * font_8x13_bold.width;
            fb_draw_text(fb, ax + (art_size - gw) / 2, ay + (art_size - font_8x13_bold.height) / 2, g, &font_8x13_bold, false);
        }

        int16_t by = fb->height - 24;
        int16_t vu_y = ay + art_size + 6;
        int16_t vu_max_h = by - vu_y - 4;
        if (vu_max_h > 46) vu_max_h = 46;
        if (vu_max_h >= 12) {
            draw_spectrum(fb, ax + 1, vu_y, vu_max_h, app);
        }

        int16_t tx = ax + art_size + 8;
        size_t cap = (size_t)((fb->width - tx - 4) / font_6x10.width);

        fb_draw_text_trunc(fb, tx, ay, cur->title, cap, &font_8x13_bold, false);
        fb_draw_text_trunc(fb, tx, ay + 15, cur->artist, cap, &font_6x10, false);
        fb_draw_text_trunc(fb, tx, ay + 27, cur->album, cap, &font_6x10, false);

        char tags[64] = {0};
        if (app->shuffle) strcat(tags, "SHUF ");
        char rpt[16];
        snprintf(rpt, sizeof(rpt), "RPT:%s ", repeat_str(app->repeat));
        strcat(tags, rpt);
        if (app->format_badge[0] != '\0') {
            strcat(tags, app->format_badge);
        }
        fb_draw_text(fb, tx, ay + 39, tags, &font_6x10, false);

        int16_t q_top = ay + 54;
        int16_t q_avail = by - 2 - q_top;
        int16_t rows = q_avail / 12;

        if (rows > 0) {
            uint16_t upcoming[5];
            size_t up_count = app_get_upcoming(app, upcoming, rows > 5 ? 5 : (size_t)rows);
            if (up_count > 0) {
                fb_draw_text(fb, tx, q_top, "UP NEXT:", &font_6x10, false);
                for (size_t k = 0; k < up_count; k++) {
                    char row_str[96];
                    uint16_t trk_idx = upcoming[k];
                    if (trk_idx < app->queue_len) {
                        snprintf(row_str, sizeof(row_str), "%u. %s", (unsigned)(k + 1), app->queue[trk_idx].title);
                        fb_draw_text_trunc(fb, tx, q_top + 10 + (int16_t)k * 11, row_str, cap, &font_6x10, false);
                    }
                }
            }
        }

        float frac = 0.0f;
        if (cur->duration_secs > 0) {
            frac = (float)(app->position_ms / 1000) / (float)cur->duration_secs;
        }
        draw_progress_bar(fb, 4, by, fb->width - 8, frac);

        char tline[32];
        char pos_str[16], dur_str[16];
        format_mmss(app->position_ms / 1000, pos_str, sizeof(pos_str));
        format_mmss(cur->duration_secs, dur_str, sizeof(dur_str));
        snprintf(tline, sizeof(tline), "%s/%s", pos_str, dur_str);
        fb_draw_text(fb, 4, by + 8, tline, &font_6x10, false);

        char vol_str[16];
        snprintf(vol_str, sizeof(vol_str), "vol %u", app->volume);
        int16_t vx = fb->width - (int16_t)strlen(vol_str) * font_6x10.width - 2;
        fb_draw_text(fb, vx, by + 8, vol_str, &font_6x10, false);
    }
}

static void render_mini_footer(framebuffer_t *fb, const app_state_t *app) {
    int16_t footer_y = get_ui_footer_y(fb);
    fb_draw_line(fb, 0, footer_y, fb->width, footer_y, true);

    const track_t *now = app_get_current_track(app);
    const char *title = now ? now->title : "--";
    uint32_t dur_secs = now ? now->duration_secs : 0;

    char l1[96];
    snprintf(l1, sizeof(l1), "%s %s", state_glyph(app->state), title);
    fb_draw_text_trunc(fb, 2, footer_y + 3, l1, (size_t)((fb->width - 32) / font_6x10.width), &font_6x10, false);

    char st[16] = {0};
    if (app->shuffle) strcat(st, "S");
    if (app->repeat != REPEAT_OFF) {
        char rpt[8];
        snprintf(rpt, sizeof(rpt), "R%s", repeat_short(app->repeat));
        strcat(st, rpt);
    }
    if (st[0] != '\0') {
        int16_t sx = fb->width - (int16_t)strlen(st) * font_6x10.width - 2;
        fb_draw_text(fb, sx, footer_y + 3, st, &font_6x10, false);
    }

    if (fb->height > 128) {
        int16_t by = fb->height - 7;
        float frac = 0.0f;
        if (dur_secs > 0) {
            frac = (float)(app->position_ms / 1000) / (float)dur_secs;
        }

        char t_str[32];
        char p_str[16], d_str[16];
        format_mmss(app->position_ms / 1000, p_str, sizeof(p_str));
        format_mmss(dur_secs, d_str, sizeof(d_str));
        snprintf(t_str, sizeof(t_str), "%s/%s", p_str, d_str);
        int16_t tw = (int16_t)strlen(t_str) * font_6x10.width;
        int16_t tx = fb->width - tw - 2;

        int16_t pbar_w = tx - 8;
        if (pbar_w > 10) {
            draw_progress_bar(fb, 2, by, pbar_w, frac);
        }
        fb_draw_text(fb, tx, by - 2, t_str, &font_6x10, false);
    }
}
