#include "ui.h"
#include "font.h"
#include "framebuffer.h"
#include "hal/hal_system.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void draw_battery(framebuffer_t *fb, const app_state_t *app);
static void draw_topbar(framebuffer_t *fb, const app_state_t *app);
static void draw_caret(framebuffer_t *fb, bool up);
static void draw_progress_bar(framebuffer_t *fb, int16_t x, int16_t y, int16_t w, float frac);
static void render_list(framebuffer_t *fb, const app_state_t *app);
static void render_now_playing(framebuffer_t *fb, const app_state_t *app);
static void render_mini_footer(framebuffer_t *fb, const app_state_t *app);

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

    fb_draw_text(fb, 2, 1, header, &font_8x13_bold, false);
    draw_battery(fb, app);
    fb_draw_line(fb, 0, UI_HEADER_Y, fb->width, UI_HEADER_Y, true);
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
    fb_draw_text(fb, tx, 1, buf, &font_6x10, false);

    int16_t bat_left = tx;

    if (pct >= 0) {
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

    uint32_t ram_bytes = get_ram_usage_bytes(app);
    uint32_t ram_kb = (ram_bytes + 1023) / 1024;
    char ram_str[16];
    snprintf(ram_str, sizeof(ram_str), "%uK", (unsigned)ram_kb);
    int16_t rw = (int16_t)strlen(ram_str) * font_6x10.width;
    int16_t rx = bat_left - rw - 6;

    fb_draw_text(fb, rx, 1, ram_str, &font_6x10, false);
}

static void draw_caret(framebuffer_t *fb, bool up) {
    int16_t x = fb->width - 6;
    int16_t y = up ? UI_BODY_TOP : (UI_FOOTER_Y - 6);

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
    fb_draw_rect(fb, x, y, w, 5, true);

    if (frac > 1.0f) frac = 1.0f;
    if (frac < 0.0f) frac = 0.0f;

    int16_t fill = (int16_t)((w - 2) * frac);
    if (fill > 0) {
        fb_fill_rect(fb, x + 1, y + 1, fill, 3, true);
    }
}

static void draw_spectrum(framebuffer_t *fb, int16_t x, int16_t y, int16_t max_h, const app_state_t *app) {
    if (!fb || !app || !app->vu_enabled) return;
    int16_t bar_w = 5;
    int16_t gap = 2;

    for (int b = 0; b < 8; b++) {
        int16_t bx = x + b * (bar_w + gap);
        uint8_t lvl = (uint8_t)(((uint32_t)app->vu_meter[b] * (uint32_t)max_h) / 24);
        if (lvl > max_h) lvl = (uint8_t)max_h;

        // Continuous baseline bar
        fb_draw_line(fb, bx, y + max_h, bx + bar_w - 1, y + max_h, true);

        // Segmented LED blocks (3px block, 1px dark gap)
        if (lvl > 0) {
            for (int16_t h = 2; h <= lvl; h += 3) {
                fb_fill_rect(fb, bx, y + (max_h - h), bar_w, 2, true);
            }
        }

        // Floating peak indicator
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

    if (n == 0) {
        const char *empty_msg = "No tracks found on /sdcard.";
        if (app->screen == SCREEN_ALBUMS) empty_msg = "No albums found.";
        else if (app->screen == SCREEN_ARTISTS) empty_msg = "No artists found.";
        fb_draw_text(fb, 8, UI_BODY_TOP + 8, empty_msg, &font_6x10, false);
    } else {
        uint16_t visible = UI_VISIBLE_ROWS;
        uint16_t max_start = (n > visible) ? (n - visible) : 0;
        uint16_t start = (sel > visible / 2) ? (sel - visible / 2) : 0;
        if (start > max_start) start = max_start;

        for (uint16_t row = 0; row < visible; row++) {
            uint16_t idx = start + row;
            if (idx >= n) break;

            int16_t y = UI_BODY_TOP + row * UI_ROW_HEIGHT;
            bool is_selected = (idx == sel);

            if (is_selected) {
                fb_fill_rect(fb, 2, y - 1, fb->width - 4, UI_ROW_HEIGHT, true);
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
                    if (idx == 0) snprintf(line, sizeof(line), "Shuffle:    [%s]", app->shuffle ? "ON" : "OFF");
                    else if (idx == 1) snprintf(line, sizeof(line), "Repeat:     [%s]", repeat_str(app->repeat));
                    else if (idx == 2) snprintf(line, sizeof(line), "Volume:        %u%%", app->volume);
                    else if (idx == 3) snprintf(line, sizeof(line), "Brightness:    %u%%", app->brightness);
                    else if (idx == 4) snprintf(line, sizeof(line), "Theme:      %s", THEMES[app->theme_index].name);
                    else if (idx == 5) snprintf(line, sizeof(line), "Output:     [%s]", (app->output_mode == OUTPUT_BLE_AUDIO) ? "BLE AUDIO" : "I2S DAC");
                    else if (idx == 6) snprintf(line, sizeof(line), "Visualizer: [%s]", app->vu_enabled ? "ON" : "OFF");
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

            fb_draw_text_trunc(fb, 6, y + 1, line, (size_t)((fb->width - 12) / font_6x10.width), &font_6x10, is_selected);

            if (app->screen == SCREEN_SETTINGS) {
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

static void render_now_playing(framebuffer_t *fb, const app_state_t *app) {
    draw_topbar(fb, app);

    const track_t *cur = app_get_current_track(app);
    if (!cur) {
        fb_draw_text(fb, 4, UI_BODY_TOP + 4, "No track playing", &font_6x10, false);
        return;
    }

#if (LCD_WIDTH <= 128)
    // Compact 128x64 OLED Layout
    fb_draw_text_trunc(fb, 2, 13, cur->title, 20, &font_6x10, false);
    fb_draw_text_trunc(fb, 2, 24, cur->artist, 20, &font_6x10, false);

    // 8-Band Equalizer & Format Tag
    draw_spectrum(fb, 2, 36, 12, app);

    char tags[32] = {0};
    if (app->shuffle) strcat(tags, "S ");
    if (app->repeat != REPEAT_OFF) strcat(tags, "R ");
    if (app->format_badge[0] != '\0') {
        strcat(tags, app->format_badge);
    }
    fb_draw_text_trunc(fb, 60, 36, tags, 11, &font_6x10, false);

    // Mini Progress Bar & Time
    int16_t by = fb->height - 13;
    float frac = 0.0f;
    if (cur->duration_secs > 0) {
        frac = (float)(app->position_ms / 1000) / (float)cur->duration_secs;
    }
    draw_progress_bar(fb, 2, by, fb->width - 4, frac);

    char tline[32];
    char pos_str[16], dur_str[16];
    format_mmss(app->position_ms / 1000, pos_str, sizeof(pos_str));
    format_mmss(cur->duration_secs, dur_str, sizeof(dur_str));
    snprintf(tline, sizeof(tline), "%s %s/%s", state_glyph(app->state), pos_str, dur_str);
    fb_draw_text(fb, 2, by + 5, tline, &font_6x10, false);

    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "v%u", app->volume);
    int16_t vx = fb->width - (int16_t)strlen(vol_str) * font_6x10.width - 2;
    fb_draw_text(fb, vx, by + 5, vol_str, &font_6x10, false);
#else
    // Wide / Full-Size Color & E-Ink Layout
    int16_t ax = UI_ART_X;
    int16_t ay = UI_ART_Y;
    int16_t art_size = ART_BOX_PX;

    // Left Column 1: Album Art Box
    if (app->art_valid && app->art_rgb565) {
        fb_draw_rect(fb, ax, ay, art_size, art_size, true);
    } else {
        fb_draw_rect(fb, ax, ay, art_size, art_size, true);
        fb_draw_rect(fb, ax + 2, ay + 2, art_size - 4, art_size - 4, true);
        const char *g = state_glyph(app->state);
        int16_t gw = (int16_t)strlen(g) * font_8x13_bold.width;
        fb_draw_text(fb, ax + (art_size - gw) / 2, ay + (art_size - font_8x13_bold.height) / 2, g, &font_8x13_bold, false);
    }

    // Left Column 2: 8-Band Segmented VU Spectrum Analyzer
    int16_t by = fb->height - 22;
    int16_t vu_y = ay + art_size + 6;
    int16_t vu_max_h = by - vu_y - 4;
    if (vu_max_h > 46) vu_max_h = 46;
    if (vu_max_h >= 12) {
        draw_spectrum(fb, ax + 1, vu_y, vu_max_h, app);
    }

    // Right Column 1: Metadata
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

    // Right Column 2: UP NEXT Queue List
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

    // Bottom Transport & Progress Bar
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
    fb_draw_text(fb, 4, by + 7, tline, &font_6x10, false);

    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "vol %u", app->volume);
    int16_t vx = fb->width - (int16_t)strlen(vol_str) * font_6x10.width - 2;
    fb_draw_text(fb, vx, by + 7, vol_str, &font_6x10, false);
#endif
}

static void render_mini_footer(framebuffer_t *fb, const app_state_t *app) {
    fb_draw_line(fb, 0, UI_FOOTER_Y, fb->width, UI_FOOTER_Y, true);

    const track_t *now = app_get_current_track(app);
    const char *title = now ? now->title : "--";
    uint32_t dur_secs = now ? now->duration_secs : 0;

    char l1[96];
    snprintf(l1, sizeof(l1), "%s %s", state_glyph(app->state), title);
    fb_draw_text_trunc(fb, 2, UI_FOOTER_Y + 3, l1, 24, &font_6x10, false);

    char st[16] = {0};
    if (app->shuffle) strcat(st, "S");
    if (app->repeat != REPEAT_OFF) {
        char rpt[8];
        snprintf(rpt, sizeof(rpt), "R%s", repeat_short(app->repeat));
        strcat(st, rpt);
    }
    if (st[0] != '\0') {
        int16_t sx = fb->width - (int16_t)strlen(st) * font_6x10.width - 2;
        fb_draw_text(fb, sx, UI_FOOTER_Y + 3, st, &font_6x10, false);
    }

    int16_t by = fb->height - 7;
    float frac = 0.0f;
    if (dur_secs > 0) {
        frac = (float)(app->position_ms / 1000) / (float)dur_secs;
    }
    draw_progress_bar(fb, 2, by, fb->width - 64, frac);

    char t_str[32];
    char p_str[16], d_str[16];
    format_mmss(app->position_ms / 1000, p_str, sizeof(p_str));
    format_mmss(dur_secs, d_str, sizeof(d_str));
    snprintf(t_str, sizeof(t_str), "%s/%s", p_str, d_str);
    int16_t tx = fb->width - (int16_t)strlen(t_str) * font_6x10.width - 2;
    fb_draw_text(fb, tx, by - 1, t_str, &font_6x10, false);
}
