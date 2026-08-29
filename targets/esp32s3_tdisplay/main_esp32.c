#if defined(ESP_PLATFORM)

#include "app.h"
#include "settings.h"
#include "audio_player.h"
#include "config.h"
#include "framebuffer.h"
#include "ui.h"
#include "themes.h"
#include "library/library.h"
#include "hal/hal_audio.h"
#include "hal/hal_display.h"
#include "hal/hal_input.h"
#include "hal/hal_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static app_state_t s_app;

static void audio_task(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        if (s_app.state == PLAYBACK_PLAYING) {
            audio_player_process();
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

void app_main(void) {
    ESP_LOGI("KOPUZ", "Kopuz Device booting on LilyGO T-Display S3...");

    hal_display_init();
    hal_input_init();

    static uint8_t mono_buf[LCD_FRAME_BYTES_1BPP];
    framebuffer_t fb;
    fb_init(&fb, mono_buf, LCD_WIDTH, LCD_HEIGHT);

    ui_render_message(&fb, "KOPUZ", "starting...");
    hal_display_flush(fb.buffer);

    hal_storage_mount();

    app_init(&s_app);
    audio_player_init(&s_app);

    library_scan(STORAGE_MOUNT_POINT, &s_app);
    settings_load(&s_app);

    // Audio decoding task on Core 1
    xTaskCreatePinnedToCore(audio_task, "audio_task", 16384, NULL, 5, NULL, 1);

    uint32_t last_progress = 0;

    while (1) {
        btn_event_t btn = hal_input_poll();
        while (btn != BTN_NONE) {
            app_command_t cmd = app_on_button(&s_app, btn);
            if (cmd != CMD_NONE) {
                audio_player_send_command(cmd);
            }
            btn = hal_input_poll();
        }

        app_check_memory_safety(&s_app);

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        bool progress_due = false;
        if (s_app.state == PLAYBACK_PLAYING && (now - last_progress >= 500)) {
            progress_due = true;
            last_progress = now;
        }

        if (s_app.dirty) {
            ui_render(&fb, &s_app);
            hal_display_flush(fb.buffer);

            if (s_app.screen == SCREEN_NOW_PLAYING && s_app.art_valid && s_app.art_rgb565) {
                hal_display_blit_rgb565(
                    UI_ART_X,
                    UI_ART_Y,
                    s_app.art_size,
                    s_app.art_size,
                    s_app.art_rgb565
                );
            }
            s_app.dirty = false;
        } else if (progress_due) {
            ui_render(&fb, &s_app);
            if (s_app.screen == SCREEN_NOW_PLAYING) {
                const uint16_t band = 26;
                hal_display_flush_region(fb.buffer, 0, LCD_HEIGHT - band, LCD_WIDTH, band);
            } else {
                const uint16_t footer_h = (LCD_HEIGHT <= 64) ? 12 : ((LCD_HEIGHT <= 128) ? 18 : 24);
                hal_display_flush_region(fb.buffer, 0, LCD_HEIGHT - footer_h, LCD_WIDTH, footer_h);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

#endif
