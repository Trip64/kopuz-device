#include "hal/hal_input.h"

#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define PIN_PLAY_PAUSE 2
#define PIN_NEXT       3
#define PIN_PREV       4
#define PIN_VOL_UP     5
#define PIN_VOL_DOWN   6

#define QUEUE_SIZE 8
static btn_event_t s_queue[QUEUE_SIZE];
static uint s_head = 0;
static uint s_tail = 0;

static uint32_t s_last_press_us[7] = {0};
static uint32_t s_play_press_time = 0;

static void queue_push(btn_event_t ev) {
    uint next = (s_head + 1) % QUEUE_SIZE;
    if (next != s_tail) {
        s_queue[s_head] = ev;
        s_head = next;
    }
}

void hal_input_init(void) {
    uint pins[] = {PIN_PLAY_PAUSE, PIN_NEXT, PIN_PREV, PIN_VOL_UP, PIN_VOL_DOWN};
    for (size_t i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
}

btn_event_t hal_input_poll(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Play/Pause with long-press detection
    static bool s_play_was_down = false;
    bool play_down = !gpio_get(PIN_PLAY_PAUSE);
    if (play_down && !s_play_was_down) {
        s_play_press_time = now;
        s_play_was_down = true;
    } else if (!play_down && s_play_was_down) {
        s_play_was_down = false;
        uint32_t held = now - s_play_press_time;
        if (held >= 500) {
            queue_push(BTN_BACK);
        } else if (held >= 30) {
            queue_push(BTN_PLAY_PAUSE);
        }
    }

    struct { uint pin; btn_event_t ev; } btns[] = {
        {PIN_NEXT, BTN_NEXT},
        {PIN_PREV, BTN_PREV},
        {PIN_VOL_UP, BTN_VOL_UP},
        {PIN_VOL_DOWN, BTN_VOL_DOWN}
    };

    for (size_t i = 0; i < sizeof(btns)/sizeof(btns[0]); i++) {
        if (!gpio_get(btns[i].pin)) {
            if (now - s_last_press_us[btns[i].pin] > 150) {
                s_last_press_us[btns[i].pin] = now;
                queue_push(btns[i].ev);
            }
        }
    }

    if (s_tail != s_head) {
        btn_event_t ev = s_queue[s_tail];
        s_tail = (s_tail + 1) % QUEUE_SIZE;
        return ev;
    }

    return BTN_NONE;
}

#endif
