#if defined(ESP_PLATFORM)

#include "app.h"
#include "audio_player.h"
#include "config.h"
#include "framebuffer.h"
#include "library/library.h"
#include "settings.h"
#include "ui.h"
#include "hal/hal_display.h"
#include "hal/hal_input.h"
#include "hal/hal_storage.h"
#if HAS_BLE_AUDIO
#include "hal/hal_ble_audio.h"
#endif

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "kopuz";
static app_state_t s_app;
static char s_serial_line[64];
static size_t s_serial_length;

static btn_event_t execute_serial_command(char *line) {
    while (*line && isspace((unsigned char)*line)) ++line;
    for (char *cursor = line; *cursor; ++cursor) {
        *cursor = (char)tolower((unsigned char)*cursor);
    }

    if (!strcmp(line, "next") || !strcmp(line, "n")) return BTN_NEXT;
    if (!strcmp(line, "previous") || !strcmp(line, "prev") || !strcmp(line, "p")) {
        return BTN_PREV;
    }
    if (!strcmp(line, "select") || !strcmp(line, "play") || !strcmp(line, "enter")) {
        return BTN_PLAY_PAUSE;
    }
    if (!strcmp(line, "back") || !strcmp(line, "b")) return BTN_BACK;
    if (!strcmp(line, "volume+") || !strcmp(line, "vol+")) return BTN_VOL_UP;
    if (!strcmp(line, "volume-") || !strcmp(line, "vol-")) return BTN_VOL_DOWN;
    if (!strncmp(line, "brightness ", 11)) {
        char *end = NULL;
        long value = strtol(line + 11, &end, 10);
        if (end != line + 11 && *end == '\0' && value >= 10 && value <= 100) {
            s_app.brightness = (uint8_t)value;
            hal_display_set_brightness(s_app.brightness);
            settings_save(&s_app);
            ESP_LOGI(TAG, "Brightness set to %ld%%", value);
        } else {
            ESP_LOGW(TAG, "Brightness must be between 10 and 100");
        }
        return BTN_NONE;
    }
#if HAS_BLE_AUDIO
    if (!strcmp(line, "bt scan")) {
        if (hal_ble_audio_init(AUDIO_DEFAULT_SAMPLE_RATE, AUDIO_CHANNELS) == 0) {
            hal_ble_audio_set_volume(s_app.volume);
            hal_ble_audio_start_scan();
            ESP_LOGI(TAG, "Bluetooth scan started; send 'bt list' after a few seconds");
        }
        return BTN_NONE;
    }
    if (!strcmp(line, "bt list")) {
        s_app.bt_device_count = hal_ble_audio_get_discovered(s_app.bt_devices, 8);
        ESP_LOGI(TAG, "Bluetooth devices: %u", (unsigned)s_app.bt_device_count);
        for (uint8_t i = 0; i < s_app.bt_device_count; ++i) {
            ESP_LOGI(TAG, "  %u: %s%s", (unsigned)i + 1, s_app.bt_devices[i].name,
                     s_app.bt_devices[i].connected ? " [connected]" : "");
        }
        return BTN_NONE;
    }
    if (!strncmp(line, "bt connect ", 11)) {
        char *end = NULL;
        long index = strtol(line + 11, &end, 10);
        if (end != line + 11 && *end == '\0' && index >= 1 && index <= 8 &&
            hal_ble_audio_connect_device((uint8_t)(index - 1))) {
            s_app.output_mode = OUTPUT_BLE_AUDIO;
            settings_save(&s_app);
            ESP_LOGI(TAG, "Connecting to Bluetooth device %ld", index);
        } else {
            ESP_LOGW(TAG, "Connection failed; run 'bt scan', then 'bt list'");
        }
        return BTN_NONE;
    }
    if (!strcmp(line, "bt disconnect")) {
        hal_ble_audio_disconnect();
        ESP_LOGI(TAG, "Bluetooth disconnect requested");
        return BTN_NONE;
    }
#endif
    if (!strcmp(line, "status")) {
        ESP_LOGI(TAG, "status screen=%d selection=%u tracks=%u playback=%d volume=%u brightness=%u",
                 (int)s_app.screen, (unsigned)app_get_current_selection(&s_app),
                 (unsigned)s_app.queue_len, (int)s_app.state,
                 (unsigned)s_app.volume, (unsigned)s_app.brightness);
        return BTN_NONE;
    }
    if (!strcmp(line, "help") || !strcmp(line, "?")) {
        ESP_LOGI(TAG, "Commands: next, prev, select, back, vol+, vol-, brightness 10..100, status");
#if HAS_BLE_AUDIO
        ESP_LOGI(TAG, "Bluetooth: bt scan, bt list, bt connect N, bt disconnect");
#endif
        return BTN_NONE;
    }
    if (*line) ESP_LOGW(TAG, "Unknown command '%s' (send 'help')", line);
    return BTN_NONE;
}

static btn_event_t poll_serial(void) {
    char byte;
    while (read(STDIN_FILENO, &byte, 1) == 1) {
        if (byte == '\r' || byte == '\n') {
            if (s_serial_length == 0) continue;
            s_serial_line[s_serial_length] = '\0';
            s_serial_length = 0;
            return execute_serial_command(s_serial_line);
        }
        if (byte == '\b' || byte == 0x7f) {
            if (s_serial_length > 0) --s_serial_length;
        } else if (isprint((unsigned char)byte) &&
                   s_serial_length < sizeof(s_serial_line) - 1) {
            s_serial_line[s_serial_length++] = byte;
        }
    }
    return BTN_NONE;
}

static void audio_task(void *argument) {
    (void)argument;
    while (true) {
        if (s_app.state == PLAYBACK_PLAYING) {
            audio_player_process();
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Kopuz starting on CrowPanel 2.4 DIS03024H V2.1");

    if (hal_display_init() != 0) {
        ESP_LOGE(TAG, "Display initialization failed; stopping bring-up");
        return;
    }
    hal_input_init();
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    static uint8_t mono_buffer[LCD_FRAME_BYTES_1BPP];
    framebuffer_t framebuffer;
    fb_init(&framebuffer, mono_buffer, LCD_WIDTH, LCD_HEIGHT);
    ui_render_message(&framebuffer, "KOPUZ / CROWPANEL", "Starting hardware checks...");
    hal_display_flush(framebuffer.buffer);

    bool storage_ready = hal_storage_mount() == 0;
    app_init(&s_app);
    audio_player_init(&s_app);

    uint16_t track_count = 0;
    if (storage_ready) track_count = library_scan(STORAGE_MOUNT_POINT, &s_app);
    settings_load(&s_app);

    char status[64];
    if (storage_ready) {
        snprintf(status, sizeof(status), "SD OK - %u track(s) found", (unsigned)track_count);
    } else {
        snprintf(status, sizeof(status), "SD failed - UI will still run");
    }
    ui_render_message(&framebuffer, "HARDWARE CHECK", status);
    hal_display_flush(framebuffer.buffer);
    vTaskDelay(pdMS_TO_TICKS(1400));

    if (xTaskCreatePinnedToCore(audio_task, "audio", 16384, NULL, 5, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "Could not create audio task");
        app_trigger_bsod(&s_app, "ERR_OUT_OF_MEMORY", "Could not start audio task");
    }

    ESP_LOGI(TAG, "Ready. Serial: send 'help'. Touch: tap select, swipe up/down move, left back");
    uint32_t last_progress_ms = 0;
    uint32_t last_bluetooth_refresh_ms = 0;

    while (true) {
        btn_event_t button = poll_serial();
        if (button == BTN_NONE) button = hal_input_poll();
        if (button != BTN_NONE) {
            app_command_t command = app_on_button(&s_app, button);
            if (command != CMD_NONE) audio_player_send_command(command);
        }

        app_check_memory_safety(&s_app);

        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
#if HAS_BLE_AUDIO
        if (s_app.screen == SCREEN_BLUETOOTH &&
            now_ms - last_bluetooth_refresh_ms >= 500) {
            last_bluetooth_refresh_ms = now_ms;
            uint8_t previous_count = s_app.bt_device_count;
            bool previous_scanning = s_app.bt_scanning;
            s_app.bt_device_count = hal_ble_audio_get_discovered(s_app.bt_devices, 8);
            s_app.bt_scanning = hal_ble_audio_is_scanning();
            if (s_app.bt_device_count != previous_count ||
                s_app.bt_scanning != previous_scanning) {
                s_app.dirty = true;
            }
        }
#endif
        bool progress_due = s_app.state == PLAYBACK_PLAYING &&
                            now_ms - last_progress_ms >= 500;
        if (progress_due) last_progress_ms = now_ms;

        if (s_app.dirty) {
            ui_render(&framebuffer, &s_app);
            hal_display_flush(framebuffer.buffer);
            if (s_app.screen == SCREEN_NOW_PLAYING && s_app.art_valid && s_app.art_rgb565) {
                hal_display_blit_rgb565(UI_ART_X, UI_ART_Y, s_app.art_size,
                                        s_app.art_size, s_app.art_rgb565);
            }
            s_app.dirty = false;
        } else if (progress_due) {
            ui_render(&framebuffer, &s_app);
            if (s_app.screen == SCREEN_NOW_PLAYING) {
                const uint16_t band_height = 26;
                hal_display_flush_region(framebuffer.buffer, 0,
                                         LCD_HEIGHT - band_height,
                                         LCD_WIDTH, band_height);
            } else {
                const uint16_t footer_height = 24;
                hal_display_flush_region(framebuffer.buffer, 0,
                                         LCD_HEIGHT - footer_height,
                                         LCD_WIDTH, footer_height);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

#endif
