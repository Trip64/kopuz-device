#include "app.h"
#include "settings.h"
#include "hal/hal_audio.h"
#include "hal/hal_display.h"
#include "hal/hal_system.h"
#include "hal/hal_ble_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static uint8_t s_last_volume;
static uint8_t s_last_brightness;
static bool s_bt_init_fails;
static bool s_bt_scanning;

bool settings_save(const app_state_t *app) { return app != NULL; }
bool settings_load(app_state_t *app) { return app != NULL; }
void hal_audio_set_volume(uint8_t volume) { s_last_volume = volume; }
void hal_audio_stop(void) {}
void hal_audio_resume(void) {}
void hal_display_set_brightness(uint8_t pct) { s_last_brightness = pct; }
void hal_display_set_theme(uint16_t fg, uint16_t bg) { (void)fg; (void)bg; }
uint32_t hal_system_random(void) { return 4; }
void hal_system_reboot(void) {}
int hal_ble_audio_init(uint32_t sample_rate, uint8_t channels) {
    (void)sample_rate;
    (void)channels;
    return s_bt_init_fails ? -1 : 0;
}
void hal_ble_audio_set_volume(uint8_t volume) { (void)volume; }
void hal_ble_audio_start_scan(void) { s_bt_scanning = true; }
bool hal_ble_audio_is_scanning(void) { return s_bt_scanning; }
uint8_t hal_ble_audio_get_discovered(bt_device_entry_t *devices, uint8_t max_count) {
    (void)devices;
    (void)max_count;
    return 0;
}
bool hal_ble_audio_connect_device(uint8_t index) { (void)index; return false; }
void hal_ble_audio_disconnect(void) {}

static track_t make_track(const char *title) {
    track_t track;
    memset(&track, 0, sizeof(track));
    snprintf(track.title, sizeof(track.title), "%s", title);
    snprintf(track.path, sizeof(track.path), "/sdcard/%s.wav", title);
    return track;
}

int main(void) {
    app_state_t app;
    app_init(&app);

    CHECK(app.queue != NULL);
    CHECK(app_get_current_track(&app) == NULL);
    CHECK(app_get_list_len(NULL) == 0);
    CHECK(app_get_current_selection(NULL) == 0);
    CHECK(app_get_battery_pct(&app) == -1);
    app_set_current_selection(&app, 3);
    CHECK(app.menu_sel == 3);

    track_t tracks[] = {
        make_track("one"),
        make_track("two"),
        make_track("three")
    };
    app_set_queue(&app, tracks, 3);
    CHECK(app.queue_len == 3);
    CHECK(app.play_order_len == 3);
    CHECK(strcmp(app_get_current_track(&app)->title, "one") == 0);
    app.screen = SCREEN_SONGS;
    app_set_current_selection(&app, 99);
    CHECK(app.songs_sel == 2);

    app.screen = SCREEN_CONFIRM_REBOOT;
    app_set_current_selection(&app, 1);
    CHECK(app.menu_sel == 1);

    app.screen = SCREEN_NOW_PLAYING;
    CHECK(app_on_button(&app, BTN_PLAY_PAUSE) == CMD_LOAD_CURRENT);
    CHECK(app.state == PLAYBACK_PLAYING);
    CHECK(app_on_button(&app, BTN_NEXT) == CMD_LOAD_CURRENT);
    CHECK(app.current_index == 1);
    CHECK(app_on_button(&app, BTN_PREV) == CMD_LOAD_CURRENT);
    CHECK(app.current_index == 0);

    app.repeat = REPEAT_OFF;
    app.play_pos = 2;
    app.current_index = 2;
    CHECK(app_on_track_end(&app) == CMD_NONE);
    CHECK(app.state == PLAYBACK_STOPPED);

    app.repeat = REPEAT_ALL;
    app.state = PLAYBACK_PLAYING;
    CHECK(app_on_track_end(&app) == CMD_LOAD_CURRENT);
    CHECK(app.current_index == 0);

    app.repeat = REPEAT_ONE;
    app.position_ms = 5000;
    CHECK(app_on_track_end(&app) == CMD_LOAD_CURRENT);
    CHECK(app.position_ms == 0);

    uint16_t upcoming[4] = {0};
    app.repeat = REPEAT_ALL;
    app.play_pos = 1;
    CHECK(app_get_upcoming(&app, upcoming, 4) == 4);
    CHECK(upcoming[0] == 2 && upcoming[1] == 0 && upcoming[2] == 1 && upcoming[3] == 2);

    app_on_button(&app, BTN_VOL_UP);
    CHECK(app.volume == 75 && s_last_volume == 75);
    app.screen = SCREEN_SETTINGS;
    app.settings_sel = 3;
    app_on_button(&app, BTN_PLAY_PAUSE);
    CHECK(s_last_brightness == app.brightness);

    app.settings_sel = 5;
    s_bt_init_fails = true;
    app_on_button(&app, BTN_PLAY_PAUSE);
    CHECK(app.output_mode == OUTPUT_I2S_DAC);
    CHECK(app.screen == SCREEN_SETTINGS);
    s_bt_init_fails = false;
    app_on_button(&app, BTN_PLAY_PAUSE);
    CHECK(app.output_mode == OUTPUT_BLE_AUDIO);
    CHECK(app.screen == SCREEN_BLUETOOTH);
    CHECK(app.bt_scanning && s_bt_scanning);

    app_set_queue(&app, NULL, 0);
    CHECK(app.queue_len == 0);
    CHECK(app.play_order_len == 0);
    CHECK(app_on_button(&app, BTN_PLAY_PAUSE) == CMD_NONE);

    app_deinit(&app);
    puts("app state tests passed");
    return 0;
}
