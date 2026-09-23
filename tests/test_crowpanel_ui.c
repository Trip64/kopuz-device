#include "app.h"
#include "framebuffer.h"
#include "ui.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *MENU_ITEMS[6] = {
    "Now Playing", "Songs", "Albums", "Artists", "Bluetooth", "Settings"
};

uint16_t app_get_list_len(const app_state_t *app) {
    if (app->screen == SCREEN_MENU) return 6;
    if (app->screen == SCREEN_SONGS) return app->queue_len;
    return 0;
}
uint16_t app_get_current_selection(const app_state_t *app) {
    return app->screen == SCREEN_MENU ? app->menu_sel : app->songs_sel;
}
void app_set_current_selection(app_state_t *app, uint16_t selection) {
    if (app->screen == SCREEN_MENU) app->menu_sel = selection;
    else if (app->screen == SCREEN_SONGS) app->songs_sel = selection;
}
const track_t *app_get_current_track(const app_state_t *app) {
    return app->queue_len && app->current_index < app->queue_len
        ? &app->queue[app->current_index] : NULL;
}
size_t app_get_upcoming(const app_state_t *app, uint16_t *out_indices, size_t max_count) {
    (void)app;
    (void)out_indices;
    (void)max_count;
    return 0;
}
int8_t app_get_battery_pct(const app_state_t *app) { (void)app; return -1; }
uint32_t hal_system_get_ram_used_bytes(void) { return 0; }
bool hal_ble_audio_is_connected(void) { return false; }
const char *hal_ble_audio_get_device_name(void) { return "Disconnected"; }
void ui_render_bsod(framebuffer_t *fb, const char *code, const char *details,
                    const char *qr_payload) {
    (void)fb;
    (void)code;
    (void)details;
    (void)qr_payload;
}

static void write_preview(const char *prefix, const char *name,
                          const framebuffer_t *fb) {
    if (!prefix) return;
    char path[512];
    snprintf(path, sizeof(path), "%s-%s.ppm", prefix, name);
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    fprintf(file, "P6\n%u %u\n255\n", fb->width, fb->height);
    for (uint16_t y = 0; y < fb->height; ++y) {
        for (uint16_t x = 0; x < fb->width; ++x) {
            uint8_t value = fb_get_pixel(fb, x, y) ? 24 : 244;
            uint8_t pixel[3] = {value, value, value};
            assert(fwrite(pixel, 1, sizeof(pixel), file) == sizeof(pixel));
        }
    }
    assert(fclose(file) == 0);
}

int main(int argc, char **argv) {
    static uint8_t pixels[LCD_FRAME_BYTES_1BPP];
    framebuffer_t fb;
    fb_init(&fb, pixels, LCD_WIDTH, LCD_HEIGHT);

    static track_t tracks[8];
    for (unsigned i = 0; i < 8; ++i) {
        snprintf(tracks[i].title, sizeof(tracks[i].title),
                 "Track %u - A long readable song title", i + 1);
        snprintf(tracks[i].artist, sizeof(tracks[i].artist), "Kopuz Trio");
        snprintf(tracks[i].album, sizeof(tracks[i].album), "First Encounter");
        tracks[i].duration_secs = 180;
    }
    app_state_t app = {0};
    app.queue = tracks;
    app.queue_len = 8;
    app.volume = 70;
    app.state = PLAYBACK_PLAYING;

    app.screen = SCREEN_MENU;
    app.menu_sel = 2;
    ui_render(&fb, &app);
    assert(fb_get_pixel(&fb, 6, 35));
    assert(fb_get_pixel(&fb, 8, 90));
    touch_event_t touch = {.gesture = TOUCH_TAP, .x = 170, .y = 148};
    assert(ui_crowpanel_touch(&app, &touch) == BTN_PLAY_PAUSE);
    assert(app.menu_sel == 5);
    write_preview(argc > 1 ? argv[1] : NULL, "menu", &fb);

    app.screen = SCREEN_SONGS;
    app.songs_sel = 4;
    ui_render(&fb, &app);
    assert(fb_get_pixel(&fb, 8, 35));
    assert(fb_get_pixel(&fb, 8, 155));
    touch.x = 25;
    touch.y = 43;
    assert(ui_crowpanel_touch(&app, &touch) == BTN_PLAY_PAUSE);
    assert(app.songs_sel == 2);
    touch.x = 200;
    touch.y = 215;
    assert(ui_crowpanel_touch(&app, &touch) == BTN_PLAY_PAUSE);
    write_preview(argc > 1 ? argv[1] : NULL, "songs", &fb);

    app.screen = SCREEN_NOW_PLAYING;
    app.position_ms = 30000;
    ui_render(&fb, &app);
    assert(fb_get_pixel(&fb, UI_ART_X - 2, UI_ART_Y - 2));
    assert(fb_get_pixel(&fb, 164, CROW_UI_NAV_Y));
    touch.x = 120;
    touch.y = 140;
    assert(ui_crowpanel_touch(&app, &touch) == BTN_VOL_DOWN);
    touch.x = 285;
    assert(ui_crowpanel_touch(&app, &touch) == BTN_VOL_UP);
    write_preview(argc > 1 ? argv[1] : NULL, "now", &fb);
    puts("CrowPanel UI layout tests passed");
    return 0;
}
