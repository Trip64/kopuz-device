#include "hal/hal_input.h"
#include "sim_display.h"
#include "sim_dac.h"
#include <SDL.h>
#include <stdbool.h>

#define EVENT_QUEUE_SIZE 16
#define REPEAT_DELAY_MS  260
#define REPEAT_RATE_MS    90

static btn_event_t s_queue[EVENT_QUEUE_SIZE];
static size_t s_q_head = 0;
static size_t s_q_tail = 0;

static btn_event_t s_held_btn = BTN_NONE;
static uint32_t s_press_start = 0;
static uint32_t s_last_repeat = 0;
extern bool g_sim_running;
extern bool g_sim_profile_changed;

static void queue_push(btn_event_t ev) {
    size_t next = (s_q_head + 1) % EVENT_QUEUE_SIZE;
    if (next != s_q_tail) {
        s_queue[s_q_head] = ev;
        s_q_head = next;
    }
}

void hal_input_init(void) {
    s_q_head = 0;
    s_q_tail = 0;
    s_held_btn = BTN_NONE;
    s_press_start = 0;
    s_last_repeat = 0;
}

btn_event_t hal_input_poll(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            g_sim_running = false;
            return BTN_NONE;
        }

        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            btn_event_t btn = BTN_NONE;
            switch (e.key.keysym.sym) {
                // Profile switching hotkeys
                case SDLK_F1:
                    hal_sim_display_set_mode(DISP_PROFILE_STM32F7_480X272);
                    g_sim_profile_changed = true;
                    break;
                case SDLK_F2:
                    hal_sim_display_set_mode(DISP_PROFILE_ILI9341_320X240);
                    g_sim_profile_changed = true;
                    break;
                case SDLK_F3:
                    hal_sim_display_set_mode(DISP_PROFILE_TDISPLAY_320X170);
                    g_sim_profile_changed = true;
                    break;
                case SDLK_F4:
                    hal_sim_display_set_mode(DISP_PROFILE_OLED_128X64);
                    g_sim_profile_changed = true;
                    break;
                case SDLK_F5:
                    hal_sim_display_set_mode(DISP_PROFILE_OLED_128X128);
                    g_sim_profile_changed = true;
                    break;
                case SDLK_F6:
                    hal_sim_display_set_mode(DISP_PROFILE_SHARP_400X240);
                    g_sim_profile_changed = true;
                    break;
                case SDLK_F7:
                    hal_sim_display_set_mode(DISP_PROFILE_EPAPER_BWR_296X128);
                    g_sim_profile_changed = true;
                    break;
                case SDLK_F8:
                    hal_sim_dac_print_status();
                    break;
                case SDLK_F9: {
                    sim_dac_model_t next_dac = (hal_sim_dac_get_model() + 1) % SIM_DAC_MODEL_COUNT;
                    hal_sim_dac_set_model(next_dac);
                    break;
                }
                case SDLK_TAB: {
                    uint8_t next = (hal_sim_display_get_mode() + 1) % hal_sim_display_get_mode_count();
                    hal_sim_display_set_mode(next);
                    g_sim_profile_changed = true;
                    break;
                }

                // Playback and navigation controls
                case SDLK_SPACE:
                case SDLK_RETURN:
                    btn = BTN_PLAY_PAUSE;
                    break;
                case SDLK_UP:
                case SDLK_k:
                    btn = BTN_PREV;
                    break;
                case SDLK_DOWN:
                case SDLK_j:
                    btn = BTN_NEXT;
                    break;
                case SDLK_LEFT:
                case SDLK_h:
                    btn = BTN_SEEK_BACK;
                    break;
                case SDLK_RIGHT:
                case SDLK_l:
                    btn = BTN_SEEK_FWD;
                    break;
                case SDLK_EQUALS:
                case SDLK_PLUS:
                case SDLK_u:
                    btn = BTN_VOL_UP;
                    break;
                case SDLK_MINUS:
                case SDLK_d:
                    btn = BTN_VOL_DOWN;
                    break;
                case SDLK_BACKSPACE:
                case SDLK_ESCAPE:
                case SDLK_b:
                    btn = BTN_BACK;
                    break;
                case SDLK_c:
                    btn = BTN_CRASH_TEST;
                    break;
            }

            if (btn != BTN_NONE) {
                queue_push(btn);
                s_held_btn = btn;
                s_press_start = SDL_GetTicks();
                s_last_repeat = s_press_start;
            }
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            uint16_t dw = 320, dh = 240;
            hal_sim_display_get_size(&dw, &dh);

            // Get window size to compute scale
            int win_w = dw, win_h = dh;
            SDL_Window *win = SDL_GetWindowFromID(e.button.windowID);
            if (win) SDL_GetWindowSize(win, &win_w, &win_h);

            int px = (e.button.x * dw) / (win_w ? win_w : 1);
            int py = (e.button.y * dh) / (win_h ? win_h : 1);

            btn_event_t btn = BTN_NONE;
            if (py < (dh * 18) / 100) {
                btn = BTN_BACK; // Top header -> Back
            } else if (px < (dw * 30) / 100) {
                btn = BTN_PREV; // Left region -> Previous track / Up
            } else if (px > (dw * 70) / 100) {
                btn = BTN_NEXT; // Right region -> Next track / Down
            } else {
                btn = BTN_PLAY_PAUSE; // Center region -> Play/Pause
            }

            if (btn != BTN_NONE) {
                queue_push(btn);
            }
        } else if (e.type == SDL_KEYUP) {
            s_held_btn = BTN_NONE;
        }
    }

    if (s_held_btn == BTN_NEXT || s_held_btn == BTN_PREV ||
        s_held_btn == BTN_VOL_UP || s_held_btn == BTN_VOL_DOWN ||
        s_held_btn == BTN_SEEK_FWD || s_held_btn == BTN_SEEK_BACK) {
        uint32_t now = SDL_GetTicks();
        if ((now - s_press_start) >= REPEAT_DELAY_MS) {
            if ((now - s_last_repeat) >= REPEAT_RATE_MS) {
                queue_push(s_held_btn);
                s_last_repeat = now;
            }
        }
    }

    if (s_q_tail != s_q_head) {
        btn_event_t ev = s_queue[s_q_tail];
        s_q_tail = (s_q_tail + 1) % EVENT_QUEUE_SIZE;
        return ev;
    }

    return BTN_NONE;
}
