#if defined(ESP_PLATFORM)

#include "hal/hal_input.h"
#include "crowpanel_pins.h"

#include "driver/gpio.h"
#include "esp_timer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBOUNCE_MS 25
#define LONG_PRESS_MS 600

typedef struct {
    gpio_num_t pin;
    bool raw_pressed;
    bool stable_pressed;
    int64_t raw_changed_ms;
    int64_t pressed_ms;
} button_state_t;

static button_state_t s_buttons[2] = {
    {.pin = CROW_BUTTON_1},
    {.pin = CROW_BUTTON_2},
};

void hal_input_init(void) {
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << CROW_BUTTON_1) | (1ULL << CROW_BUTTON_2),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&config);

    int64_t now = esp_timer_get_time() / 1000;
    for (size_t i = 0; i < 2; ++i) {
        s_buttons[i].raw_pressed = gpio_get_level(s_buttons[i].pin) == 1;
        s_buttons[i].stable_pressed = s_buttons[i].raw_pressed;
        s_buttons[i].raw_changed_ms = now;
        s_buttons[i].pressed_ms = now;
    }
}

static btn_event_t poll_button(button_state_t *button, btn_event_t short_event,
                               btn_event_t long_event, int64_t now) {
    bool pressed = gpio_get_level(button->pin) == 1;
    if (pressed != button->raw_pressed) {
        button->raw_pressed = pressed;
        button->raw_changed_ms = now;
    }

    if (button->stable_pressed == button->raw_pressed ||
        now - button->raw_changed_ms < DEBOUNCE_MS) {
        return BTN_NONE;
    }

    button->stable_pressed = button->raw_pressed;
    if (button->stable_pressed) {
        button->pressed_ms = now;
        return BTN_NONE;
    }

    return now - button->pressed_ms >= LONG_PRESS_MS ? long_event : short_event;
}

btn_event_t hal_input_poll(void) {
    int64_t now = esp_timer_get_time() / 1000;

    /* Left: move forward/backward. Right: select/back. */
    btn_event_t event = poll_button(&s_buttons[0], BTN_NEXT, BTN_PREV, now);
    if (event != BTN_NONE) return event;
    return poll_button(&s_buttons[1], BTN_PLAY_PAUSE, BTN_BACK, now);
}

#endif
