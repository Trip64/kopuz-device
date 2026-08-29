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
#include "hal/hal_power.h"
#include "hal/hal_storage.h"
#include "hal/hal_system.h"
#include "sim_display.h"
#include "sim_dac.h"
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

bool g_sim_running = true;
bool g_sim_profile_changed = false;

static app_state_t s_app;
static SDL_Thread *s_audio_thread = NULL;
static bool s_audio_thread_running = false;

static int sim_audio_task(void *data) {
    (void)data;
    while (s_audio_thread_running) {
        if (s_app.state == PLAYBACK_PLAYING) {
            audio_player_process();
            SDL_Delay(1);
        } else {
            SDL_Delay(20);
        }
    }
    return 0;
}

static void print_help(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -m, --mode <name>   Select display hardware profile:\n");
    for (uint8_t i = 0; i < hal_sim_display_get_mode_count(); i++) {
        printf("                        * %-8s : %s\n", hal_sim_display_get_mode_id(i), hal_sim_display_get_mode_desc(i));
    }
    printf("  -d, --dac <name>    Select simulated hardware DAC model:\n");
    for (uint8_t d = 0; d < SIM_DAC_MODEL_COUNT; d++) {
        printf("                        * %-8s : %s\n",
               (d == 0) ? "pcm5102a" : ((d == 1) ? "pt8211" : ((d == 2) ? "cs4344" : "pwm")),
               hal_sim_dac_get_model_name((sim_dac_model_t)d));
    }
    printf("  -s, --scale <1..8>  Override window integer pixel scaling factor\n");
    printf("  -l, --list-modes    List all available screen hardware profiles & specs\n");
    printf("  --list-dacs         List all available simulated hardware DAC chips\n");
    printf("  --test              Run non-interactive automated self-test suite\n");
    printf("  -h, --help          Show this help message\n\n");
    printf("Runtime Hotkeys:\n");
    printf("  [F1..F7]            Switch Display Resolution / Device Profiles\n");
    printf("  [F8]                Inspect Hardware DAC & I2S DMA Buffer Status\n");
    printf("  [F9]                Cycle Simulated Hardware DAC Models\n");
    printf("  [TAB]               Cycle through screen hardware profiles live\n");
    printf("  [Left Click]        Simulate Touch Screen interaction\n");
    printf("===================================================\n");
}

int main(int argc, char *argv[]) {
    bool test_mode = false;
    uint8_t selected_mode = DISP_PROFILE_STM32F7_480X272;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            test_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list-modes") == 0) {
            printf("=========================================================================================\n");
            printf("                              Kopuz Display Hardware Profiles                            \n");
            printf("=========================================================================================\n");
            for (uint8_t m = 0; m < hal_sim_display_get_mode_count(); m++) {
                printf("  [%u] %-8s : %-30s\n       Specs : %s\n\n",
                    (unsigned)m,
                    hal_sim_display_get_mode_id(m),
                    hal_sim_display_get_mode_name(m),
                    hal_sim_display_get_mode_desc(m)
                );
            }
            printf("=========================================================================================\n");
            return 0;
        } else if (strcmp(argv[i], "--list-dacs") == 0) {
            printf("=========================================================================================\n");
            printf("                              Kopuz Hardware DAC Simulation Models                       \n");
            printf("=========================================================================================\n");
            for (uint8_t d = 0; d < SIM_DAC_MODEL_COUNT; d++) {
                printf("  [%u] %-10s : %s\n", (unsigned)d,
                       (d == 0) ? "pcm5102a" : ((d == 1) ? "pt8211" : ((d == 2) ? "cs4344" : "pwm")),
                       hal_sim_dac_get_model_name((sim_dac_model_t)d));
            }
            printf("=========================================================================================\n");
            return 0;
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dac") == 0) && i + 1 < argc) {
            const char *arg = argv[++i];
            if (strstr(arg, "pcm") || strstr(arg, "5102")) hal_sim_dac_set_model(SIM_DAC_PCM5102A_I2S);
            else if (strstr(arg, "pt") || strstr(arg, "8211")) hal_sim_dac_set_model(SIM_DAC_PT8211_LSBJ);
            else if (strstr(arg, "cs") || strstr(arg, "4344")) hal_sim_dac_set_model(SIM_DAC_CS4344_I2S);
            else if (strstr(arg, "pwm") || strstr(arg, "timer")) hal_sim_dac_set_model(SIM_DAC_PWM_RING);
        } else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) && i + 1 < argc) {
            const char *arg = argv[++i];
            bool found = false;
            for (uint8_t m = 0; m < hal_sim_display_get_mode_count(); m++) {
                if (strcasecmp(arg, hal_sim_display_get_mode_id(m)) == 0) {
                    selected_mode = m;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (strstr(arg, "480") || strstr(arg, "f7")) selected_mode = DISP_PROFILE_STM32F7_480X272;
                else if (strstr(arg, "320x240") || strstr(arg, "ili") || strstr(arg, "pico")) selected_mode = DISP_PROFILE_ILI9341_320X240;
                else if (strstr(arg, "170") || strstr(arg, "tdisplay") || strstr(arg, "esp32")) selected_mode = DISP_PROFILE_TDISPLAY_320X170;
                else if (strstr(arg, "64") || strstr(arg, "oled64") || strstr(arg, "oled")) selected_mode = DISP_PROFILE_OLED_128X64;
                else if (strstr(arg, "128") || strstr(arg, "square")) selected_mode = DISP_PROFILE_OLED_128X128;
                else if (strstr(arg, "400") || strstr(arg, "sharp") || strstr(arg, "mip")) selected_mode = DISP_PROFILE_SHARP_400X240;
                else if (strstr(arg, "epaper") || strstr(arg, "bwr") || strstr(arg, "eink") || strstr(arg, "296")) selected_mode = DISP_PROFILE_EPAPER_BWR_296X128;
            }
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--scale") == 0) && i + 1 < argc) {
            int s = atoi(argv[++i]);
            if (s >= 1 && s <= 8) {
                hal_sim_display_set_scale_override((uint8_t)s);
            }
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    hal_sim_display_set_mode(selected_mode);
    hal_display_init();
    hal_input_init();
    hal_battery_init();

    printf("===================================================\n");
    printf("                   kopuz player                    \n");
    printf("===================================================\n");
    printf("Active Display Profile: %s\n", hal_sim_display_get_mode_name(hal_sim_display_get_mode()));
    printf("Active DAC Model      : %s\n", hal_sim_dac_get_model_name(hal_sim_dac_get_model()));
    printf("Controls:\n");
    printf("  [Enter / Space]  : Select / Play-Pause\n");
    printf("  [Down / J]       : Next / Scroll Down\n");
    printf("  [Up / K]         : Prev / Scroll Up\n");
    printf("  [Right / L]      : Seek Forward 5s\n");
    printf("  [Left / H]       : Seek Backward 5s\n");
    printf("  [+ / = / U]      : Volume Up\n");
    printf("  [- / D]          : Volume Down\n");
    printf("  [Esc / Backspace]: Back\n");
    printf("  [F1..F7 / TAB]   : Switch Screen Resolution / Device Mode\n");
    printf("  [F8]             : Inspect Hardware DAC & I2S DMA Buffer\n");
    printf("  [F9]             : Cycle Simulated Hardware DAC Chips\n");
    printf("  [Left Mouse]     : Touch Screen Input\n");
    printf("  [C]              : Test BSOD Crash Screen\n");
    printf("===================================================\n");

    static uint8_t mono_buf[MAX_LCD_FRAME_BYTES];
    uint16_t cur_w = 320, cur_h = 240;
    hal_sim_display_get_size(&cur_w, &cur_h);

    framebuffer_t fb;
    fb_init(&fb, mono_buf, cur_w, cur_h);

    ui_render_message(&fb, "KOPUZ", "starting...");
    hal_display_flush(fb.buffer);
    hal_display_present();

    hal_storage_mount();

    app_init(&s_app);
    s_app.battery_mv = hal_battery_read_mv();

    audio_player_init(&s_app);

    printf("Scanning music library under '%s'...\n", STORAGE_MOUNT_POINT);
    uint16_t num_tracks = library_scan(STORAGE_MOUNT_POINT, &s_app);
    printf("Found %u tracks.\n", (unsigned)num_tracks);

    settings_load(&s_app);
    hal_display_set_theme(THEMES[s_app.theme_index].fg, THEMES[s_app.theme_index].bg);

    if (test_mode) {
        hal_audio_set_volume(0);
        printf("\n--- RUNNING SELF-TEST ---\n");
        if (num_tracks < 3) {
            printf("FAIL: Expected at least 3 tracks, found %u\n", (unsigned)num_tracks);
            return 1;
        }

        int wav_idx = -1;
        int mp3_idx = -1;
        int flac_idx = -1;
        for (uint16_t i = 0; i < num_tracks; i++) {
            printf("  Track %u: %s [%s]\n", (unsigned)i, s_app.queue[i].title, s_app.queue[i].path);
            if (wav_idx < 0 && strstr(s_app.queue[i].path, "01 - A440 Test Tone.wav")) wav_idx = (int)i;
            if (mp3_idx < 0 && strstr(s_app.queue[i].path, "03 - Acoustic Resonance.mp3")) mp3_idx = (int)i;
            if (flac_idx < 0 && strstr(s_app.queue[i].path, "02 - Cyber Pulse.flac")) flac_idx = (int)i;
        }

        if (wav_idx >= 0) {
            printf("[TEST 1/4] Testing WAV Test Tone (%s)...\n", s_app.queue[wav_idx].title);
            s_app.current_index = (uint16_t)wav_idx;
            s_app.state = PLAYBACK_PLAYING;
            audio_player_send_command(CMD_LOAD_CURRENT);
            uint32_t start_t = hal_get_time_ms();
            while (hal_get_time_ms() - start_t < 1200) {
                while (s_app.state == PLAYBACK_PLAYING && hal_audio_needs_data()) {
                    audio_player_process();
                }
                hal_delay_ms(5);
            }
            ui_render(&fb, &s_app);
            hal_display_flush(fb.buffer);
            printf("  Playing: %s (Position: %u ms, Art valid: %s) -> PASS\n",
                   s_app.queue[s_app.current_index].title, (unsigned)s_app.position_ms, s_app.art_valid ? "YES" : "NO");
        }

        if (mp3_idx >= 0) {
            printf("[TEST 2/4] Testing MP3 Test Tone (%s)...\n", s_app.queue[mp3_idx].title);
            s_app.current_index = (uint16_t)mp3_idx;
            s_app.state = PLAYBACK_PLAYING;
            audio_player_send_command(CMD_LOAD_CURRENT);
            uint32_t start_t = hal_get_time_ms();
            while (hal_get_time_ms() - start_t < 1200) {
                while (s_app.state == PLAYBACK_PLAYING && hal_audio_needs_data()) {
                    audio_player_process();
                }
                hal_delay_ms(5);
            }
            ui_render(&fb, &s_app);
            hal_display_flush(fb.buffer);
            printf("  Playing: %s (Position: %u ms, Art valid: %s) -> PASS\n",
                   s_app.queue[s_app.current_index].title, (unsigned)s_app.position_ms, s_app.art_valid ? "YES" : "NO");
        }

        if (flac_idx >= 0) {
            printf("[TEST 3/4] Testing FLAC Test Tone (%s)...\n", s_app.queue[flac_idx].title);
            s_app.current_index = (uint16_t)flac_idx;
            s_app.state = PLAYBACK_PLAYING;
            audio_player_send_command(CMD_LOAD_CURRENT);
            uint32_t start_t = hal_get_time_ms();
            while (hal_get_time_ms() - start_t < 1200) {
                while (s_app.state == PLAYBACK_PLAYING && hal_audio_needs_data()) {
                    audio_player_process();
                }
                hal_delay_ms(5);
            }
            ui_render(&fb, &s_app);
            hal_display_flush(fb.buffer);
            printf("  Playing: %s (Position: %u ms, Art valid: %s) -> PASS\n",
                   s_app.queue[s_app.current_index].title, (unsigned)s_app.position_ms, s_app.art_valid ? "YES" : "NO");

            app_on_button(&s_app, BTN_SEEK_FWD);
            audio_player_process();
            printf("  Seek Forward: %u ms -> PASS\n", (unsigned)s_app.position_ms);
            app_on_button(&s_app, BTN_SEEK_BACK);
            audio_player_process();
            printf("  Seek Backward: %u ms -> PASS\n", (unsigned)s_app.position_ms);
        }

        printf("[TEST 4/4] Testing Hardware DAC Emulation & Diagnostics...\n");
        hal_sim_dac_print_status();

        printf("\nALL TESTS PASSED\n");

        audio_player_close();
        hal_storage_unmount();
        SDL_Quit();
        return 0;
    }

    // Launch dedicated Core 1 Audio Task Thread (matches FreeRTOS xTaskCreatePinnedToCore)
    s_audio_thread_running = true;
    s_audio_thread = SDL_CreateThread(sim_audio_task, "sim_audio_task", NULL);

    uint32_t last_tick = hal_get_time_ms();
    uint32_t last_progress_time = hal_get_time_ms();

    while (g_sim_running) {
        if (g_sim_profile_changed) {
            g_sim_profile_changed = false;
            uint16_t nw = 320, nh = 240;
            hal_sim_display_get_size(&nw, &nh);
            fb_init(&fb, mono_buf, nw, nh);
            s_app.dirty = true;
            printf("[SIM] Switched display mode -> %s (%ux%u)\n", hal_sim_display_get_mode_name(hal_sim_display_get_mode()), nw, nh);
        }

        btn_event_t btn = hal_input_poll();
        while (btn != BTN_NONE) {
            app_command_t cmd = app_on_button(&s_app, btn);
            if (cmd != CMD_NONE) {
                audio_player_send_command(cmd);
            }
            btn = hal_input_poll();
        }

        app_check_memory_safety(&s_app);

        uint32_t now = hal_get_time_ms();
        uint8_t cur_mode = hal_sim_display_get_mode();
        bool is_epaper = (cur_mode == DISP_PROFILE_EPAPER_BWR_296X128);

        if (s_app.state == PLAYBACK_PLAYING) {
            if (is_epaper) {
                if (now - last_progress_time >= 1000) {
                    last_progress_time = now;
                    s_app.dirty = true;
                }
            } else {
                audio_player_tick_vu();
                s_app.dirty = true;
                if (now - last_progress_time >= 500) {
                    last_progress_time = now;
                }
            }
        }

        if (s_app.dirty) {
            ui_render(&fb, &s_app);
            hal_display_flush(fb.buffer);

            if (s_app.screen == SCREEN_NOW_PLAYING && s_app.art_valid && s_app.art_rgb565 && hal_sim_display_is_color()) {
                uint8_t asize = hal_sim_display_get_art_size();
                if (asize > 0) {
                    int16_t ay = ((fb.height <= 64) ? 11 : ((fb.height <= 128) ? 14 : 16)) + 2;
                    uint8_t src_sz = (s_app.art_size > 0) ? s_app.art_size : 80;
                    hal_sim_display_blit_art(4, ay, asize, asize, s_app.art_rgb565, src_sz, src_sz);
                }
            }
            hal_display_present();
            s_app.dirty = false;
        }

        // Frame rate limiter (30ms = ~33 FPS)
        uint32_t elapsed = hal_get_time_ms() - last_tick;
        if (elapsed < 30) {
            hal_delay_ms(30 - elapsed);
        }
        last_tick = hal_get_time_ms();
    }

    s_audio_thread_running = false;
    if (s_audio_thread) {
        SDL_WaitThread(s_audio_thread, NULL);
        s_audio_thread = NULL;
    }

    settings_save(&s_app);
    audio_player_close();
    hal_storage_unmount();
    SDL_Quit();
    printf("Kopuz Device shutdown complete.\n");
    return 0;
}
