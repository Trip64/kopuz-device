#include "hal/hal_input.h"
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
