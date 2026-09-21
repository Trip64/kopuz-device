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

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "kopuz";
static app_state_t s_app;

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

    if (xTaskCreatePinnedToCore(audio_task, "audio", 12288, NULL, 5, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "Could not create audio task");
        app_trigger_bsod(&s_app, "ERR_OUT_OF_MEMORY", "Could not start audio task");
    }

    ESP_LOGI(TAG, "Ready. Left short/long: next/previous; right short/long: select/back");
    uint32_t last_progress_ms = 0;

    while (true) {
        btn_event_t button = hal_input_poll();
        if (button != BTN_NONE) {
            app_command_t command = app_on_button(&s_app, button);
            if (command != CMD_NONE) audio_player_send_command(command);
        }

        app_check_memory_safety(&s_app);

        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
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
