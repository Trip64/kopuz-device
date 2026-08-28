#include "app.h"
#include "hal/hal_audio.h"
#include "hal/hal_display.h"
#include "hal/hal_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *MENU_ITEMS[5] = {"Now Playing", "Songs", "Albums", "Artists", "Settings"};
const char *SETTINGS_ITEMS[6] = {"Shuffle", "Repeat", "Volume", "Brightness", "Theme", "Output"};

static void shuffle_keep_first(uint16_t *order, uint16_t n, uint16_t chosen_pos) {
    if (n <= 1) return;
    if (chosen_pos >= n) chosen_pos = 0;
    uint16_t chosen = order[chosen_pos];

    // Fisher-Yates shuffle
    for (int i = (int)n - 1; i > 0; i--) {
        int j = (int)(hal_random() % (uint32_t)(i + 1));
        uint16_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    // Move chosen track to index 0
    for (uint16_t i = 0; i < n; i++) {
        if (order[i] == chosen) {
            uint16_t tmp = order[0];
            order[0] = order[i];
            order[i] = tmp;
            break;
        }
    }
}

void app_init(app_state_t *app) {
    memset(app, 0, sizeof(app_state_t));

    app->queue_cap = MAX_TRACKS;
    app->queue = (track_t*)calloc(app->queue_cap, sizeof(track_t));
    if (!app->queue) {
        app_trigger_bsod(app, "ERR_OUT_OF_MEMORY", "Track queue alloc failed");
        return;
    }

    app->volume = 70;
    app->state = PLAYBACK_STOPPED;
    app->screen = SCREEN_MENU;
    app->shuffle = false;
    app->repeat = REPEAT_OFF;
    app->brightness = 100;
    app->theme_index = 0;
    app->battery_mv = -1;
    app->dirty = true;

    // Allocate album art box buffer (RGB565)
    app->art_size = ART_BOX_PX;
    app->art_rgb565 = (uint8_t*)malloc((size_t)ART_BOX_PX * ART_BOX_PX * 2);
    if (!app->art_rgb565) {
        app_trigger_bsod(app, "ERR_OUT_OF_MEMORY", "Art buffer alloc failed");
        return;
    }
    app->art_valid = false;
}

const track_t* app_get_current_track(const app_state_t *app) {
    if (!app || app->queue_len == 0 || app->current_index >= app->queue_len) {
        return NULL;
    }
    return &app->queue[app->current_index];
}

int8_t app_get_battery_pct(const app_state_t *app) {
    if (!app || app->battery_mv < 3000) {
        return -1; // USB / not connected
    }
    int32_t pct = ((app->battery_mv - 3300) * 100) / (4200 - 3300);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (int8_t)pct;
}

uint16_t app_get_list_len(const app_state_t *app) {
    switch (app->screen) {
        case SCREEN_MENU:         return 5;
        case SCREEN_NOW_PLAYING:  return 0;
        case SCREEN_SONGS:        return app->queue_len;
        case SCREEN_ALBUMS:       return app->albums_len;
        case SCREEN_ARTISTS:      return app->artists_len;
        case SCREEN_ALBUM_TRACKS:
            return (app->open_group < app->albums_len) ? app->albums[app->open_group].count : 0;
        case SCREEN_ARTIST_TRACKS:
            return (app->open_group < app->artists_len) ? app->artists[app->open_group].count : 0;
        case SCREEN_SETTINGS:     return 5;
        case SCREEN_BSOD:         return 0;
    }
    return 0;
}

uint16_t app_get_current_selection(const app_state_t *app) {
    switch (app->screen) {
        case SCREEN_MENU:         return app->menu_sel;
        case SCREEN_NOW_PLAYING:  return 0;
        case SCREEN_SONGS:        return app->songs_sel;
        case SCREEN_ALBUMS:       return app->albums_sel;
        case SCREEN_ARTISTS:      return app->artists_sel;
        case SCREEN_ALBUM_TRACKS:
        case SCREEN_ARTIST_TRACKS:return app->group_sel;
        case SCREEN_SETTINGS:     return app->settings_sel;
        case SCREEN_BSOD:         return 0;
    }
    return 0;
}

static uint16_t* get_sel_ptr(app_state_t *app) {
    switch (app->screen) {
        case SCREEN_MENU:         return &app->menu_sel;
        case SCREEN_SONGS:        return &app->songs_sel;
        case SCREEN_ALBUMS:       return &app->albums_sel;
        case SCREEN_ARTISTS:      return &app->artists_sel;
        case SCREEN_ALBUM_TRACKS:
        case SCREEN_ARTIST_TRACKS:return &app->group_sel;
        case SCREEN_NOW_PLAYING:
        case SCREEN_SETTINGS:     return &app->settings_sel;
        case SCREEN_BSOD:         return &app->menu_sel;
    }
    return &app->menu_sel;
}

static void move_selection(app_state_t *app, int16_t delta) {
    uint16_t n = app_get_list_len(app);
    if (n == 0) return;

    uint16_t *sel = get_sel_ptr(app);
    int32_t next = (int32_t)*sel + delta;
    if (next < 0) {
        next = (n > 0) ? (n - 1) : 0;
    } else if (next >= n) {
        next = 0;
    }
    *sel = (uint16_t)next;
}

static void go_back(app_state_t *app) {
    switch (app->screen) {
        case SCREEN_ALBUM_TRACKS:
            app->screen = SCREEN_ALBUMS;
            break;
        case SCREEN_ARTIST_TRACKS:
            app->screen = SCREEN_ARTISTS;
            break;
        default:
            app->screen = SCREEN_MENU;
            break;
    }
}

static bool ensure_play_order(app_state_t *app) {
    if (app->play_order_len > 0) return true;
    if (app->queue_len == 0) return false;

    if (app->play_order) free(app->play_order);
    app->play_order = (uint16_t*)malloc(app->queue_len * sizeof(uint16_t));
    for (uint16_t i = 0; i < app->queue_len; i++) {
        app->play_order[i] = i;
    }
    app->play_order_len = app->queue_len;
    app->play_pos = (app->current_index < app->queue_len) ? app->current_index : 0;
    return true;
}

static app_command_t skip_track(app_state_t *app, int16_t delta) {
    if (!ensure_play_order(app)) return CMD_NONE;

    int32_t n = (int32_t)app->play_order_len;
    int32_t pos = (int32_t)app->play_pos + delta;
    if (pos < 0) pos = n - 1;
    else if (pos >= n) pos = 0;

    app->play_pos = (uint16_t)pos;
    app->current_index = app->play_order[app->play_pos];
    app->position_ms = 0;
    app->state = PLAYBACK_PLAYING;
    app->dirty = true;
    return CMD_LOAD_CURRENT;
}

static app_command_t toggle_play(app_state_t *app) {
    if (app->queue_len == 0) return CMD_NONE;

    switch (app->state) {
        case PLAYBACK_PLAYING:
            app->state = PLAYBACK_PAUSED;
            return CMD_PAUSE;
        case PLAYBACK_PAUSED:
            app->state = PLAYBACK_PLAYING;
            return CMD_PLAY;
        case PLAYBACK_STOPPED:
            if (!ensure_play_order(app)) return CMD_NONE;
            app->current_index = app->play_order[app->play_pos];
            app->state = PLAYBACK_PLAYING;
            app->position_ms = 0;
            return CMD_LOAD_CURRENT;
    }
    return CMD_NONE;
}

static app_command_t start_playlist(app_state_t *app, const uint16_t *tracks, uint16_t count, uint16_t chosen_pos) {
    if (count == 0 || !tracks) return CMD_NONE;
    if (chosen_pos >= count) chosen_pos = 0;

    uint16_t master = tracks[chosen_pos];
    if (master == app->current_index && app->play_order_len > 0) {
        if (app->state == PLAYBACK_PLAYING) {
            app->state = PLAYBACK_PAUSED;
            return CMD_PAUSE;
        } else if (app->state == PLAYBACK_PAUSED) {
            app->state = PLAYBACK_PLAYING;
            return CMD_PLAY;
        }
    }

    if (app->play_order) free(app->play_order);
    app->play_order = (uint16_t*)malloc(count * sizeof(uint16_t));
    memcpy(app->play_order, tracks, count * sizeof(uint16_t));
    app->play_order_len = count;

    if (app->shuffle) {
        shuffle_keep_first(app->play_order, count, chosen_pos);
        app->play_pos = 0;
    } else {
        app->play_pos = chosen_pos;
    }

    app->current_index = app->play_order[app->play_pos];
    app->position_ms = 0;
    app->state = PLAYBACK_PLAYING;
    return CMD_LOAD_CURRENT;
}

static void toggle_setting(app_state_t *app) {
    switch (app->settings_sel) {
        case 0: // Shuffle
            app->shuffle = !app->shuffle;
            break;
        case 1: // Repeat
            if (app->repeat == REPEAT_OFF) app->repeat = REPEAT_ALL;
            else if (app->repeat == REPEAT_ALL) app->repeat = REPEAT_ONE;
            else app->repeat = REPEAT_OFF;
            break;
        case 2: // Volume
            app->volume = (app->volume >= 100) ? 0 : (app->volume + 10);
            if (app->volume > 100) app->volume = 100;
            hal_audio_set_volume(app->volume);
            break;
        case 3: // Brightness
            app->brightness = (app->brightness >= 100) ? 20 : (app->brightness + 20);
            if (app->brightness > 100) app->brightness = 100;
            hal_display_set_brightness(app->brightness);
            break;
        case 4: // Theme
            app->theme_index = (app->theme_index + 1) % THEMES_COUNT;
            hal_display_set_theme(THEMES[app->theme_index].fg, THEMES[app->theme_index].bg);
            break;
        case 5: // Output mode
            app->output_mode = (app->output_mode == OUTPUT_I2S_DAC) ? OUTPUT_BLE_AUDIO : OUTPUT_I2S_DAC;
            break;
    }
}

static app_command_t select_item(app_state_t *app) {
    switch (app->screen) {
        case SCREEN_MENU:
            switch (app->menu_sel) {
                case 0: app->screen = SCREEN_NOW_PLAYING; break;
                case 1: app->screen = SCREEN_SONGS; break;
                case 2: app->screen = SCREEN_ALBUMS; break;
                case 3: app->screen = SCREEN_ARTISTS; break;
                case 4: app->screen = SCREEN_SETTINGS; break;
            }
            return CMD_NONE;

        case SCREEN_NOW_PLAYING:
            return toggle_play(app);

        case SCREEN_SONGS: {
            uint16_t *order = (uint16_t*)malloc(app->queue_len * sizeof(uint16_t));
            if (!order && app->queue_len > 0) {
                app_trigger_bsod(app, "ERR_OUT_OF_MEMORY", "Failed allocating playlist");
                return CMD_NONE;
            }
            for (uint16_t i = 0; i < app->queue_len; i++) order[i] = i;
            app_command_t cmd = start_playlist(app, order, app->queue_len, app->songs_sel);
            free(order);
            return cmd;
        }

        case SCREEN_ALBUMS:
            app->open_group = app->albums_sel;
            app->group_sel = 0;
            app->screen = SCREEN_ALBUM_TRACKS;
            return CMD_NONE;

        case SCREEN_ARTISTS:
            app->open_group = app->artists_sel;
            app->group_sel = 0;
            app->screen = SCREEN_ARTIST_TRACKS;
            return CMD_NONE;

        case SCREEN_ALBUM_TRACKS:
            if (app->open_group < app->albums_len) {
                track_group_t *g = &app->albums[app->open_group];
                return start_playlist(app, g->track_indices, g->count, app->group_sel);
            }
            return CMD_NONE;

        case SCREEN_ARTIST_TRACKS:
            if (app->open_group < app->artists_len) {
                track_group_t *g = &app->artists[app->open_group];
                return start_playlist(app, g->track_indices, g->count, app->group_sel);
            }
            return CMD_NONE;

        case SCREEN_SETTINGS:
            toggle_setting(app);
            return CMD_NONE;

        case SCREEN_BSOD:
            return CMD_NONE;
    }
    return CMD_NONE;
}

#if defined(TARGET_SIMULATOR)
typedef struct {
    const char *stop_code;
    const char *details;
} bsod_sample_t;

static const bsod_sample_t BSOD_SAMPLES[] = {
    {"MANUAL_CRASH_DEMO", "User pressed 'C' to test BSOD"},
    {"ERR_OUT_OF_MEMORY", "Heap allocation failed (0 bytes free)"},
    {"ERR_RAM_THRESHOLD_EXCEEDED", "Internal SRAM exceeded 240 KB"},
    {"ERR_HEAP_EXHAUSTED", "Free DMA heap dropped below 20 KB"},
    {"ERR_UNSUPPORTED_FLAC", "192kHz sample rate (max 48k on M0+)"},
    {"ERR_FILE_CORRUPT_OR_UNSUPPORTED", "Invalid sync word or magic header"},
    {"ERR_STREAM_DECODE_FAILED", "Corrupt audio stream / CRC mismatch"},
    {"ERR_UNSUPPORTED_CHANNELS", "Surround 5.1 stream (max 2 channels)"},
    {"ERR_INVALID_SAMPLE_RATE", "Sample rate out of bounds (384000 Hz)"},
    {"ERR_SD_COMM_TIMEOUT", "SDMMC CMD18 read timeout on sector 0x4F02"},
    {"ERR_I2S_DMA_UNDERRUN", "I2S DMA buffer starving (FIFO underflow)"},
    {"ERR_STACK_OVERFLOW", "Core 1 stack canary corrupted"}
};

static int s_bsod_idx = 0;
#endif

void app_trigger_bsod(app_state_t *app, const char *stop_code, const char *details) {
    if (!app) return;
    app->screen = SCREEN_BSOD;
    snprintf(app->stop_code, sizeof(app->stop_code), "%s", stop_code ? stop_code : "FATAL_ERROR");
    snprintf(app->crash_details, sizeof(app->crash_details), "%s", details ? details : "");
    app->state = PLAYBACK_STOPPED;
    app->dirty = true;

#if defined(TARGET_SIMULATOR)
    int num_samples = (int)(sizeof(BSOD_SAMPLES) / sizeof(BSOD_SAMPLES[0]));
    for (int i = 0; i < num_samples; i++) {
        if (strcmp(BSOD_SAMPLES[i].stop_code, app->stop_code) == 0) {
            s_bsod_idx = i;
            break;
        }
    }
#endif
}

app_command_t app_on_button(app_state_t *app, btn_event_t btn) {
    if (!app) return CMD_NONE;
    app->dirty = true;

    if (app->screen == SCREEN_BSOD) {
#if defined(TARGET_SIMULATOR)
        int num_samples = (int)(sizeof(BSOD_SAMPLES) / sizeof(BSOD_SAMPLES[0]));
        if (btn == BTN_NEXT) {
            s_bsod_idx = (s_bsod_idx + 1) % num_samples;
            snprintf(app->stop_code, sizeof(app->stop_code), "%s", BSOD_SAMPLES[s_bsod_idx].stop_code);
            snprintf(app->crash_details, sizeof(app->crash_details), "%s", BSOD_SAMPLES[s_bsod_idx].details);
            app->dirty = true;
            return CMD_NONE;
        } else if (btn == BTN_PREV) {
            s_bsod_idx = (s_bsod_idx - 1 + num_samples) % num_samples;
            snprintf(app->stop_code, sizeof(app->stop_code), "%s", BSOD_SAMPLES[s_bsod_idx].stop_code);
            snprintf(app->crash_details, sizeof(app->crash_details), "%s", BSOD_SAMPLES[s_bsod_idx].details);
            app->dirty = true;
            return CMD_NONE;
        }
#endif
        if (btn != BTN_NONE) {
            app->screen = SCREEN_MENU;
            hal_display_set_theme(THEMES[app->theme_index].fg, THEMES[app->theme_index].bg);
            app->dirty = true;
            return CMD_NONE;
        }
        return CMD_NONE;
    }

    switch (btn) {
        case BTN_VOL_UP:
            app->volume = (app->volume + 5 <= 100) ? (app->volume + 5) : 100;
            hal_audio_set_volume(app->volume);
            return CMD_NONE;

        case BTN_VOL_DOWN:
            app->volume = (app->volume >= 5) ? (app->volume - 5) : 0;
            hal_audio_set_volume(app->volume);
            return CMD_NONE;

        case BTN_NEXT:
            if (app->screen == SCREEN_NOW_PLAYING) {
                return skip_track(app, 1);
            } else {
                move_selection(app, 1);
                return CMD_NONE;
            }

        case BTN_PREV:
            if (app->screen == SCREEN_NOW_PLAYING) {
                return skip_track(app, -1);
            } else {
                move_selection(app, -1);
                return CMD_NONE;
            }

        case BTN_BACK:
            go_back(app);
            return CMD_NONE;

        case BTN_PLAY_PAUSE:
            return select_item(app);

        case BTN_SEEK_FWD:
            return (app->state == PLAYBACK_PLAYING || app->state == PLAYBACK_PAUSED) ? CMD_SEEK_FWD : CMD_NONE;

        case BTN_SEEK_BACK:
            return (app->state == PLAYBACK_PLAYING || app->state == PLAYBACK_PAUSED) ? CMD_SEEK_BACK : CMD_NONE;

        case BTN_CRASH_TEST:
            app_trigger_bsod(app, "MANUAL_CRASH_DEMO", "User pressed 'C' to test BSOD");
            return CMD_PAUSE;

        case BTN_NONE:
        default:
            return CMD_NONE;
    }
}

app_command_t app_on_track_end(app_state_t *app) {
    if (!app || app->play_order_len == 0) {
        if (app) app->state = PLAYBACK_STOPPED;
        return CMD_NONE;
    }

    if (app->repeat == REPEAT_ONE) {
        app->position_ms = 0;
        return CMD_LOAD_CURRENT;
    }

    if (app->play_pos + 1 < app->play_order_len) {
        app->play_pos++;
    } else if (app->repeat == REPEAT_ALL) {
        app->play_pos = 0;
    } else {
        app->state = PLAYBACK_STOPPED;
        app->dirty = true;
        return CMD_NONE;
    }

    app->current_index = app->play_order[app->play_pos];
    app->position_ms = 0;
    app->state = PLAYBACK_PLAYING;
    app->dirty = true;
    return CMD_LOAD_CURRENT;
}

size_t app_get_upcoming(const app_state_t *app, uint16_t *out_indices, size_t max_count) {
    if (!app || !out_indices || app->play_order_len == 0 || max_count == 0) {
        return 0;
    }

    size_t count = 0;
    uint16_t len = app->play_order_len;

    for (size_t k = 1; k <= max_count; k++) {
        size_t idx = (size_t)app->play_pos + k;
        if (idx < len) {
            out_indices[count++] = app->play_order[idx];
        } else if (app->repeat == REPEAT_ALL) {
            out_indices[count++] = app->play_order[idx % len];
        } else {
            break;
        }
    }
    return count;
}

void app_check_memory_safety(app_state_t *app) {
    if (!app || app->screen == SCREEN_BSOD) return;

#if defined(PICO_RP2040)
    uint32_t ram = hal_system_get_ram_used_bytes();
    if (ram > 240 * 1024) {
        char msg[64];
        snprintf(msg, sizeof(msg), "%uKB used > 240KB threshold", (unsigned)(ram / 1024));
        app_trigger_bsod(app, "ERR_RAM_THRESHOLD_EXCEEDED", msg);
    }
#elif defined(ESP_PLATFORM)
    size_t free_heap = (size_t)(327680 - hal_system_get_ram_used_bytes());
    if (free_heap < 20480) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Free heap %uB < 20KB limit", (unsigned)free_heap);
        app_trigger_bsod(app, "ERR_HEAP_EXHAUSTED", msg);
    }
#endif
}
