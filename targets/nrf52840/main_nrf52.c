#if defined(NRF52840_XXAA) || defined(TARGET_NRF52840)

#include "app.h"
#include "audio_player.h"
#include "library/library.h"
#include "ui.h"
#include "hal/hal_audio.h"
#include "hal/hal_display.h"
#include "hal/hal_input.h"
#include "hal/hal_storage.h"
#include "hal/hal_power.h"
#include "hal/hal_system.h"

int main(void) {
    hal_display_init();
    hal_input_init();
    hal_battery_init();
    hal_storage_mount();

    static uint8_t s_framebuf[LCD_FRAME_BYTES_1BPP];
    framebuffer_t fb;
    fb_init(&fb, s_framebuf, LCD_WIDTH, LCD_HEIGHT);

    static app_state_t s_app;
    app_init(&s_app);
    s_app.battery_mv = hal_battery_read_mv();

    audio_player_init(&s_app);
    library_scan(STORAGE_MOUNT_POINT, &s_app);

    hal_display_set_theme(THEMES[s_app.theme_index].fg, THEMES[s_app.theme_index].bg);

    while (1) {
        btn_event_t btn = hal_input_poll();
        while (btn != BTN_NONE) {
            app_command_t cmd = app_on_button(&s_app, btn);
            if (cmd != CMD_NONE) {
                audio_player_send_command(cmd);
            }
            btn = hal_input_poll();
        }

        if (s_app.state == PLAYBACK_PLAYING && hal_audio_needs_data()) {
            audio_player_process();
        }

        audio_player_tick_vu();

        if (s_app.dirty) {
            ui_render(&fb, &s_app);
            hal_display_flush(fb.buffer);
            if (s_app.screen == SCREEN_NOW_PLAYING && s_app.art_valid && s_app.art_rgb565) {
                hal_display_blit_rgb565(UI_ART_X, UI_ART_Y, s_app.art_size, s_app.art_size, s_app.art_rgb565);
            }
            hal_display_present();
            s_app.dirty = false;
        }

        hal_delay_ms(10);
    }

    return 0;
}

#endif
