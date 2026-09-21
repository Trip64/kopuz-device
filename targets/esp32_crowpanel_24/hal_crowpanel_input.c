#if defined(ESP_PLATFORM)

#include "hal/hal_input.h"
#include "crowpanel_pins.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBOUNCE_MS 25
#define LONG_PRESS_MS 600
#define TOUCH_CLOCK_HZ (2 * 1000 * 1000)
#define TOUCH_SWIPE_THRESHOLD 500

static const char *TAG = "crow_input";
static spi_device_handle_t s_touch;
static bool s_touch_down;
static uint16_t s_touch_start_x;
static uint16_t s_touch_start_y;
static uint16_t s_touch_last_x;
static uint16_t s_touch_last_y;

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

    gpio_config_t irq_config = {
        .pin_bit_mask = 1ULL << CROW_TOUCH_IRQ,
        .mode = GPIO_MODE_INPUT,
        /* GPIO39 is input-only and has no internal pull resistor. */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&irq_config);

    spi_device_interface_config_t touch_config = {
        .clock_speed_hz = TOUCH_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = CROW_TOUCH_CS,
        .queue_size = 1,
    };
    esp_err_t error = spi_bus_add_device(CROW_LCD_HOST, &touch_config, &s_touch);
    if (error != ESP_OK) {
        s_touch = NULL;
        ESP_LOGE(TAG, "XPT2046 initialization failed: %s", esp_err_to_name(error));
    } else {
        ESP_LOGI(TAG, "XPT2046 touch ready on shared HSPI at %d MHz",
                 TOUCH_CLOCK_HZ / 1000000);
    }

    int64_t now = esp_timer_get_time() / 1000;
    for (size_t i = 0; i < 2; ++i) {
        s_buttons[i].raw_pressed = gpio_get_level(s_buttons[i].pin) == 1;
        s_buttons[i].stable_pressed = s_buttons[i].raw_pressed;
        s_buttons[i].raw_changed_ms = now;
        s_buttons[i].pressed_ms = now;
    }
}

static uint16_t touch_axis(uint8_t command) {
    uint8_t tx[3] = {command, 0, 0};
    uint8_t rx[3] = {0};
    spi_transaction_t transaction = {
        .length = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    if (spi_device_polling_transmit(s_touch, &transaction) != ESP_OK) return 0;
    return (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3);
}

static void touch_position(uint16_t *x, uint16_t *y) {
    uint32_t x_sum = 0;
    uint32_t y_sum = 0;
    for (unsigned sample = 0; sample < 5; ++sample) {
        x_sum += touch_axis(0xD0);
        y_sum += touch_axis(0x90);
    }
    *x = (uint16_t)(x_sum / 5);
    *y = (uint16_t)(y_sum / 5);
}

static btn_event_t poll_touch(void) {
    if (!s_touch) return BTN_NONE;
    bool pressed = gpio_get_level(CROW_TOUCH_IRQ) == 0;
    if (pressed) {
        uint16_t x;
        uint16_t y;
        touch_position(&x, &y);
        if (!s_touch_down) {
            s_touch_down = true;
            s_touch_start_x = x;
            s_touch_start_y = y;
        }
        s_touch_last_x = x;
        s_touch_last_y = y;
        return BTN_NONE;
    }
    if (!s_touch_down) return BTN_NONE;

    s_touch_down = false;
    int x_delta = (int)s_touch_start_x - (int)s_touch_last_x;
    int y_delta = (int)s_touch_start_y - (int)s_touch_last_y;
    ESP_LOGI(TAG, "Touch gesture raw start=%u,%u delta=%d,%d",
             s_touch_start_x, s_touch_start_y, x_delta, y_delta);

    if (x_delta > TOUCH_SWIPE_THRESHOLD) return BTN_BACK;
    if (y_delta > TOUCH_SWIPE_THRESHOLD) return BTN_NEXT;
    if (y_delta < -TOUCH_SWIPE_THRESHOLD) return BTN_PREV;
    return BTN_PLAY_PAUSE;
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
    event = poll_button(&s_buttons[1], BTN_PLAY_PAUSE, BTN_BACK, now);
    if (event != BTN_NONE) return event;
    return poll_touch();
}

#endif
