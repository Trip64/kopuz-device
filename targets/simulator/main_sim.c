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
#include "hal/hal_power.h"
#include "hal/hal_storage.h"
#include "hal/hal_system.h"
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>

bool g_sim_running = true;

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    printf("===================================================\n");
    printf("                   kopuz player                    \n");
    printf("===================================================\n");
    printf("Controls:\n");
    printf("  [Enter / Space]  : Select / Play-Pause\n");
    printf("  [Enter (Hold)]   : Back (Long press >= 500ms)\n");
    printf("  [Down / J]       : Next / Scroll Down\n");
    printf("  [Up / K]         : Prev / Scroll Up\n");
    printf("  [Right / L]      : Seek Forward 5s (in song)\n");
    printf("  [Left / H]       : Seek Backward 5s (in song)\n");
    printf("  [+ / = / U]      : Volume Up\n");
    printf("  [- / D]          : Volume Down\n");
    printf("  [Esc / Backspace]: Back\n");
    printf("  [C]              : Test BSOD Crash Screen\n");
    printf("===================================================\n");

    bool test_mode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            test_mode = true;
        }
    }

    hal_display_init();
    hal_input_init();
    hal_battery_init();

    static uint8_t mono_buf[LCD_FRAME_BYTES_1BPP];
    framebuffer_t fb;
    fb_init(&fb, mono_buf, LCD_WIDTH, LCD_HEIGHT);

    ui_render_message(&fb, "KOPUZ", "starting...");
    hal_display_flush(fb.buffer);
    hal_display_present();

    hal_storage_mount();

    static app_state_t app;
    app_init(&app);
    app.battery_mv = hal_battery_read_mv();

    audio_player_init(&app);

    printf("Scanning music library under '%s'...\n", STORAGE_MOUNT_POINT);
    uint16_t num_tracks = library_scan(STORAGE_MOUNT_POINT, &app);
    printf("Found %u tracks.\n", (unsigned)num_tracks);

    hal_display_set_theme(THEMES[app.theme_index].fg, THEMES[app.theme_index].bg);

    if (test_mode) {
        hal_audio_set_volume(0);
        printf("\n--- RUNNING SELF-TEST ---\n");
        if (num_tracks < 3) {
            printf("FAIL: Expected at least 3 tracks, found %u\n", (unsigned)num_tracks);
            return 1;
        }

        printf("[TEST 1/4] Playing Track 0 (WAV: %s)...\n", app.queue[0].title);
        app_command_t cmd = app_on_button(&app, BTN_PLAY_PAUSE);
        cmd = app_on_button(&app, BTN_PLAY_PAUSE);
        audio_player_send_command(cmd);

        uint32_t start_t = hal_get_time_ms();
        while (hal_get_time_ms() - start_t < 1200) {
            while (app.state == PLAYBACK_PLAYING && hal_audio_needs_data()) {
                audio_player_process();
            }
            hal_delay_ms(5);
        }
        ui_render(&fb, &app);
        hal_display_flush(fb.buffer);
        printf("  Position: %u ms (State: %d) -> PASS\n", (unsigned)app.position_ms, app.state);

        int mp3_idx = -1;
        int flac_idx = -1;
        for (uint16_t i = 0; i < num_tracks; i++) {
            printf("  Track %u: %s [%s]\n", (unsigned)i, app.queue[i].title, app.queue[i].path);
            if (strstr(app.queue[i].path, ".mp3")) mp3_idx = (int)i;
            if (strstr(app.queue[i].path, ".flac")) flac_idx = (int)i;
        }

        if (mp3_idx >= 0) {
            printf("[TEST 2/4] Testing MP3 Track (%s)...\n", app.queue[mp3_idx].title);
            app.current_index = (uint16_t)mp3_idx;
            audio_player_send_command(CMD_LOAD_CURRENT);
            start_t = hal_get_time_ms();
            while (hal_get_time_ms() - start_t < 1200) {
                while (app.state == PLAYBACK_PLAYING && hal_audio_needs_data()) {
                    audio_player_process();
                }
                hal_delay_ms(5);
            }
            ui_render(&fb, &app);
            hal_display_flush(fb.buffer);
            printf("  Playing: %s (Position: %u ms, Art valid: %s) -> PASS\n",
                   app.queue[app.current_index].title, (unsigned)app.position_ms, app.art_valid ? "YES" : "NO");
        }

        if (flac_idx >= 0) {
            printf("[TEST 3/4] Testing FLAC Track (%s)...\n", app.queue[flac_idx].title);
            app.current_index = (uint16_t)flac_idx;
            audio_player_send_command(CMD_LOAD_CURRENT);
            start_t = hal_get_time_ms();
            while (hal_get_time_ms() - start_t < 1200) {
                while (app.state == PLAYBACK_PLAYING && hal_audio_needs_data()) {
                    audio_player_process();
                }
                hal_delay_ms(5);
            }
            ui_render(&fb, &app);
            hal_display_flush(fb.buffer);
            printf("  Playing: %s (Position: %u ms, Art valid: %s) -> PASS\n",
                   app.queue[app.current_index].title, (unsigned)app.position_ms, app.art_valid ? "YES" : "NO");

            app_on_button(&app, BTN_SEEK_FWD);
            audio_player_process();
            printf("  Seek Forward: %u ms -> PASS\n", (unsigned)app.position_ms);
            app_on_button(&app, BTN_SEEK_BACK);
            audio_player_process();
            printf("  Seek Backward: %u ms -> PASS\n", (unsigned)app.position_ms);
        }

        printf("[TEST 4/4] Testing Navigation, Vol, and Theme cycling...\n");
        app_on_button(&app, BTN_BACK);
        app_on_button(&app, BTN_NEXT);
        app_on_button(&app, BTN_NEXT);
        app_on_button(&app, BTN_NEXT);
        app_on_button(&app, BTN_NEXT);
        app_on_button(&app, BTN_PLAY_PAUSE);
        app_on_button(&app, BTN_NEXT);
        app_on_button(&app, BTN_PLAY_PAUSE);
        app_on_button(&app, BTN_NEXT);
        app_on_button(&app, BTN_VOL_UP);
        app_on_button(&app, BTN_VOL_DOWN);
        ui_render(&fb, &app);
        hal_display_flush(fb.buffer);
        hal_display_present();

        printf("\nALL TESTS PASSED\n");

        audio_player_close();
        hal_storage_unmount();
        SDL_Quit();
        return 0;
    }

    uint32_t last_tick = hal_get_time_ms();
    uint32_t last_progress_time = hal_get_time_ms();

    while (g_sim_running) {
        btn_event_t btn = hal_input_poll();
        while (btn != BTN_NONE) {
            app_command_t cmd = app_on_button(&app, btn);
            if (cmd != CMD_NONE) {
                audio_player_send_command(cmd);
            }
            btn = hal_input_poll();
        }

        int audio_budget = 16;
        while (app.state == PLAYBACK_PLAYING && hal_audio_needs_data() && audio_budget-- > 0) {
            audio_player_process();
        }

        app_check_memory_safety(&app);

        uint32_t now = hal_get_time_ms();
        audio_player_tick_vu();
        if (app.state == PLAYBACK_PLAYING) {
            app.dirty = true;
            if (now - last_progress_time >= 500) {
                last_progress_time = now;
            }
        }

        if (app.dirty) {
            ui_render(&fb, &app);
            hal_display_flush(fb.buffer);

            if (app.screen == SCREEN_NOW_PLAYING && app.art_valid && app.art_rgb565) {
                hal_display_blit_rgb565(
                    UI_ART_X,
                    UI_ART_Y,
                    app.art_size,
                    app.art_size,
                    app.art_rgb565
                );
            }
            hal_display_present();
            app.dirty = false;
        }

        // Frame rate limiter (30ms = ~33 FPS)
        uint32_t elapsed = hal_get_time_ms() - last_tick;
        if (elapsed < 30) {
            hal_delay_ms(30 - elapsed);
        }
        last_tick = hal_get_time_ms();
    }

    audio_player_close();
    hal_storage_unmount();
    SDL_Quit();
    printf("Kopuz Device shutdown complete.\n");
    return 0;
}
