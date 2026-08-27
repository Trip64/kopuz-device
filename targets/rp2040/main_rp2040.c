#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "app.h"
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
#include <stdio.h>

static app_state_t s_app;

// Core 1 dedicated audio thread
static void core1_audio_entry(void) {
    while (1) {
        audio_player_process();
        tight_loop_contents();
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(1000);
    printf("Kopuz Device booting on RP2040/RP2350...\n");

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
    hal_display_set_theme(THEMES[s_app.theme_index].fg, THEMES[s_app.theme_index].bg);

    // Launch dedicated Core 1 audio decode thread
    multicore_launch_core1(core1_audio_entry);

    uint32_t last_progress = 0;

    while (1) {
        // Poll buttons
        btn_event_t btn = hal_input_poll();
        while (btn != BTN_NONE) {
            app_command_t cmd = app_on_button(&s_app, btn);
            if (cmd != CMD_NONE) {
                audio_player_send_command(cmd);
            }
            btn = hal_input_poll();
        }

        app_check_memory_safety(&s_app);

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (s_app.state == PLAYBACK_PLAYING && (now - last_progress >= 500)) {
            s_app.dirty = true;
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
        }

        sleep_ms(16);
    }
    return 0;
}

#endif
