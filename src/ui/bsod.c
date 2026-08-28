#include "ui.h"
#include "font.h"
#include "framebuffer.h"
#include "hal/hal_display.h"
#include "hal/hal_system.h"
#include "qrcodegen.h"
#include <stdio.h>
#include <string.h>

#define BSOD_COLOR_BLUE  0x03DA
#define BSOD_COLOR_WHITE 0xFFFF

static void draw_wrapped_line(framebuffer_t *fb, int16_t x, int16_t *y, const char *str, int16_t max_w, int max_lines, const font_t *font) {
    if (!fb || !str || !str[0] || !font || max_w <= 0) return;
    int chars_per_line = max_w / font->width;
    if (chars_per_line < 4) chars_per_line = 4;

    const char *p = str;
    int line = 0;

    while (*p && line < max_lines) {
        while (*p == ' ') p++;
        if (!*p) break;

        size_t rem = strlen(p);
        if ((int)rem <= chars_per_line) {
            fb_draw_text_trunc(fb, x, *y, p, (size_t)chars_per_line, font, false);
            *y += font->height + 2;
            break;
        }

        if (line == max_lines - 1) {
            fb_draw_text_trunc(fb, x, *y, p, (size_t)chars_per_line, font, false);
            *y += font->height + 2;
            break;
        }

        int break_at = chars_per_line;
        int last_space = -1;
        int last_delim = -1;

        for (int i = 0; i <= chars_per_line; i++) {
            if (p[i] == ' ') last_space = i;
            else if (p[i] == '_' || p[i] == '-' || p[i] == '/') last_delim = i + 1;
        }

        if (last_space > chars_per_line / 3) {
            break_at = last_space;
        } else if (last_delim > chars_per_line / 3) {
            break_at = last_delim;
        }

        char line_buf[64];
        int copy_len = (break_at < (int)sizeof(line_buf) - 1) ? break_at : (int)sizeof(line_buf) - 1;
        if (copy_len > chars_per_line) copy_len = chars_per_line;
        strncpy(line_buf, p, copy_len);
        line_buf[copy_len] = 0;

        fb_draw_text_trunc(fb, x, *y, line_buf, (size_t)chars_per_line, font, false);
        *y += font->height + 2;
        line++;

        p += break_at;
    }
}

void ui_render_bsod(framebuffer_t *fb, const char *stop_code, const char *details, const char *qr_payload) {
    if (!fb) return;

    hal_display_set_theme(BSOD_COLOR_WHITE, BSOD_COLOR_BLUE);
    fb_clear(fb);

    const char *sc = stop_code ? stop_code : "UNKNOWN_ERROR";

    // 1. Compact 128x64 OLED Layout
    if (fb->width <= 140 && fb->height <= 80) {
        fb_fill_rect(fb, 0, 0, fb->width, 10, true);
        fb_draw_text(fb, 2, 0, ":( CRASH", &font_6x10, true);

        fb_draw_text_trunc(fb, 2, 12, sc, 20, &font_6x10, false);

        if (details && details[0]) {
            fb_draw_text_trunc(fb, 2, 23, details, 20, &font_6x10, false);
        } else {
            fb_draw_text(fb, 2, 23, "kopuz.org/err", &font_6x10, false);
        }

        fb_draw_line(fb, 0, 36, fb->width, 36, true);
        fb_draw_text(fb, 2, 39, "Reboot needed", &font_6x10, false);

        fb_fill_rect(fb, 0, 52, fb->width, 12, true);
        fb_draw_text(fb, 2, 53, "KEY TO REBOOT", &font_6x10, true);
        return;
    }

    // 2. Square 128x128 OLED Layout
    if (fb->width <= 140 && fb->height > 80) {
        fb_draw_text(fb, 2, 2, ":( CRASH", &font_8x13_bold, false);
        fb_draw_line(fb, 0, 16, fb->width, 16, true);

        int16_t ty = 20;
        fb_draw_text(fb, 2, ty, "STOP CODE:", &font_6x10, false);
        ty += 11;
        draw_wrapped_line(fb, 2, &ty, sc, fb->width - 4, 2, &font_6x10);

        ty += 2;
        if (details && details[0]) {
            fb_draw_text(fb, 2, ty, "DETAILS:", &font_6x10, false);
            ty += 11;
            draw_wrapped_line(fb, 2, &ty, details, fb->width - 4, 2, &font_6x10);
        }

        char url[128];
        if (qr_payload && qr_payload[0]) {
            snprintf(url, sizeof(url), "%s", qr_payload);
        } else {
            snprintf(url, sizeof(url), "https://kopuz.org/err?c=%s", sc);
        }

        uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(3)];
        uint8_t tempBuffer[qrcodegen_BUFFER_LEN_FOR_VERSION(3)];
        if (qrcodegen_encodeText(url, tempBuffer, qrcode, qrcodegen_Ecc_LOW, 1, 3, qrcodegen_Mask_AUTO, true)) {
            int qr_size = qrcodegen_getSize(qrcode);
            int box_w = qr_size + 4;
            int16_t qr_x = (fb->width - box_w) / 2;
            int16_t qr_y = fb->height - box_w - 16;
            if (qr_y >= ty + 2) {
                fb_fill_rect(fb, qr_x, qr_y, box_w, box_w, true);
                for (int qy = 0; qy < qr_size; qy++) {
                    for (int qx = 0; qx < qr_size; qx++) {
                        if (qrcodegen_getModule(qrcode, qx, qy)) {
                            fb_set_pixel(fb, qr_x + 2 + qx, qr_y + 2 + qy, false);
                        }
                    }
                }
            }
        }

        fb_draw_line(fb, 0, fb->height - 14, fb->width, fb->height - 14, true);
        fb_draw_text(fb, 2, fb->height - 11, "KEY TO REBOOT", &font_6x10, false);
        return;
    }

    // 3. Wide / Standard Screens (296x128 E-Paper, 320x170 T-Display, 320x240, 400x240, 480x272)
    char url[128];
    if (qr_payload && qr_payload[0]) {
        snprintf(url, sizeof(url), "%s", qr_payload);
    } else {
        snprintf(url, sizeof(url), "https://kopuz.org/err?c=%s", sc);
    }

    uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    bool ok = qrcodegen_encodeText(
        url,
        tempBuffer,
        qrcode,
        qrcodegen_Ecc_LOW,
        1, 5,
        qrcodegen_Mask_AUTO,
        true
    );

    int16_t text_left = 12;

    if (ok) {
        int qr_size = qrcodegen_getSize(qrcode);

        int box_w = (fb->height <= 140) ? 64 : ((fb->height <= 180) ? 76 : 104);
        int box_h = box_w;
        int16_t box_x = 8;
        int16_t box_y = (fb->height - box_h) / 2;
        if (box_y < 4) box_y = 4;

        fb_fill_rect(fb, box_x, box_y, box_w, box_h, true);

        int scale = (box_w - 4) / qr_size;
        if (scale < 1) scale = 1;
        if (scale > 3) scale = 3;

        int qr_pixels = qr_size * scale;
        int16_t qr_x = box_x + (box_w - qr_pixels) / 2;
        int16_t qr_y = box_y + (box_h - qr_pixels) / 2;

        for (int qy = 0; qy < qr_size; qy++) {
            for (int qx = 0; qx < qr_size; qx++) {
                if (qrcodegen_getModule(qrcode, qx, qy)) {
                    fb_fill_rect(
                        fb,
                        qr_x + qx * scale,
                        qr_y + qy * scale,
                        scale,
                        scale,
                        false
                    );
                }
            }
        }

        text_left = box_x + box_w + 10;
    }

    int16_t right_w = fb->width - text_left - 8;
    if (right_w < 60) right_w = 60;

    int16_t ty = (fb->height <= 140) ? 4 : 8;

    const font_t *title_font = (fb->height <= 140) ? &font_6x10 : &font_8x13_bold;
    fb_draw_text(fb, text_left, ty, ":(  SYSTEM CRASH", title_font, false);
    ty += title_font->height + 3;
    fb_draw_line(fb, text_left, ty, fb->width - 8, ty, true);
    ty += 4;

    fb_draw_text(fb, text_left, ty, "STOP CODE:", &font_6x10, false);
    ty += 11;

    const font_t *sc_font = (fb->height <= 140) ? &font_6x10 : &font_8x13_bold;
    draw_wrapped_line(fb, text_left, &ty, sc, right_w, 2, sc_font);
    ty += 1;

    if (details && details[0] && ty < fb->height - 24) {
        fb_draw_text(fb, text_left, ty, "DETAILS:", &font_6x10, false);
        ty += 10;
        draw_wrapped_line(fb, text_left, &ty, details, right_w, 2, &font_6x10);
    }

    int16_t reboot_y = fb->height - 14;
    fb_draw_line(fb, text_left, reboot_y - 2, fb->width - 8, reboot_y - 2, true);
    fb_draw_text(fb, text_left, reboot_y, "PRESS ANY KEY TO REBOOT", &font_6x10, false);
}
