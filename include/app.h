#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"
#include "themes.h"
#include "hal/hal_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char path[MAX_PATH_LEN];
    char title[MAX_TITLE_LEN];
    char artist[MAX_NAME_LEN];
    char album[MAX_NAME_LEN];
    uint32_t duration_secs;
} track_t;

typedef struct {
    char name[MAX_NAME_LEN];
    uint16_t *track_indices;
    uint16_t count;
    uint16_t capacity;
} track_group_t;

typedef enum {
    PLAYBACK_STOPPED = 0,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED
} playback_state_t;

typedef enum {
    CMD_NONE = 0,
    CMD_PLAY,
    CMD_PAUSE,
    CMD_LOAD_CURRENT,
    CMD_SEEK_FWD,
    CMD_SEEK_BACK
} app_command_t;

typedef enum {
    SCREEN_MENU = 0,
    SCREEN_NOW_PLAYING,
    SCREEN_SONGS,
    SCREEN_ALBUMS,
    SCREEN_ALBUM_TRACKS,
    SCREEN_ARTISTS,
    SCREEN_ARTIST_TRACKS,
    SCREEN_SETTINGS,
    SCREEN_BSOD
} screen_t;

typedef enum {
    REPEAT_OFF = 0,
    REPEAT_ALL,
    REPEAT_ONE
} repeat_mode_t;

typedef enum {
    OUTPUT_I2S_DAC = 0,
    OUTPUT_BLE_AUDIO
} audio_output_mode_t;

typedef struct {
    track_t *queue;
    uint16_t queue_len;
    uint16_t queue_cap;

    track_group_t *albums;
    uint16_t albums_len;
    track_group_t *artists;
    uint16_t artists_len;

    uint16_t current_index;
    playback_state_t state;
    uint32_t position_ms;
    uint8_t volume;

    uint16_t *play_order;
    uint16_t play_order_len;
    uint16_t play_pos;

    screen_t screen;
    uint16_t menu_sel;
    uint16_t songs_sel;
    uint16_t albums_sel;
    uint16_t artists_sel;
    uint16_t group_sel;
    uint16_t open_group;
    uint16_t settings_sel;

    bool shuffle;
    repeat_mode_t repeat;
    uint8_t brightness;
    uint8_t theme_index;

    int32_t battery_mv;

    // Album art decoded buffer (RGB565)
    uint8_t *art_rgb565;
    uint16_t art_size;
    bool art_valid;

    // Visualizer and audio metadata
    uint8_t vu_meter[8];
    uint8_t vu_peak[8];
    char format_badge[24];
    uint16_t marquee_offset;
    uint32_t last_anim_ms;

    bool dirty;
    bool progress_due;

    audio_output_mode_t output_mode;
    bool vu_enabled;

    // Crash / BSOD fields
    char stop_code[32];
    char crash_details[64];
} app_state_t;

extern const char *MENU_ITEMS[5];
extern const char *SETTINGS_ITEMS[7];

void app_init(app_state_t *app);
void app_set_queue(app_state_t *app, track_t *tracks, uint16_t count);
app_command_t app_on_button(app_state_t *app, btn_event_t btn);
app_command_t app_on_track_end(app_state_t *app);
void app_trigger_bsod(app_state_t *app, const char *stop_code, const char *details);
void app_check_memory_safety(app_state_t *app);
const track_t* app_get_current_track(const app_state_t *app);
int8_t app_get_battery_pct(const app_state_t *app); // -1 for USB, 0..100 %
uint16_t app_get_list_len(const app_state_t *app);
uint16_t app_get_current_selection(const app_state_t *app);
size_t app_get_upcoming(const app_state_t *app, uint16_t *out_indices, size_t max_count);

#ifdef __cplusplus
}
#endif
