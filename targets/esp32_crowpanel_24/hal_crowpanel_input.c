#if defined(ESP_PLATFORM)

#include "hal/hal_input.h"
#include "config.h"
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
#define TOUCH_SWIPE_THRESHOLD_PX 32

/* Elecrow's calibration for DIS03024H with the display in rotation 1. */
#define TOUCH_X_RAW_MIN 557
#define TOUCH_X_RAW_SPAN 3263
#define TOUCH_Y_RAW_MIN 369
#define TOUCH_Y_RAW_SPAN 3493

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

static uint16_t clamp_map(int32_t value, int32_t raw_min, int32_t raw_span,
                          uint16_t pixel_span, bool invert) {
    int32_t mapped = ((value - raw_min) * pixel_span) / raw_span;
    if (mapped < 0) mapped = 0;
    if (mapped >= pixel_span) mapped = pixel_span - 1;
    return invert ? (uint16_t)(pixel_span - 1 - mapped) : (uint16_t)mapped;
}

static void touch_to_screen(uint16_t raw_x, uint16_t raw_y,
                            uint16_t *screen_x, uint16_t *screen_y) {
    /* Calibration flag 3 means swapped axes with display X inverted. */
    *screen_x = clamp_map(raw_y, TOUCH_X_RAW_MIN, TOUCH_X_RAW_SPAN, LCD_WIDTH, true);
    *screen_y = clamp_map(raw_x, TOUCH_Y_RAW_MIN, TOUCH_Y_RAW_SPAN, LCD_HEIGHT, false);
}

bool hal_input_poll_touch(touch_event_t *event) {
    if (!event || !s_touch) return false;
    event->gesture = TOUCH_NONE;
    bool pressed = gpio_get_level(CROW_TOUCH_IRQ) == 0;
    if (pressed) {
        uint16_t raw_x;
        uint16_t raw_y;
        uint16_t x;
        uint16_t y;
        touch_position(&raw_x, &raw_y);
        touch_to_screen(raw_x, raw_y, &x, &y);
        if (!s_touch_down) {
            s_touch_down = true;
            s_touch_start_x = x;
            s_touch_start_y = y;
        }
        s_touch_last_x = x;
        s_touch_last_y = y;
        return false;
    }
    if (!s_touch_down) return false;

    s_touch_down = false;
    int x_delta = (int)s_touch_last_x - (int)s_touch_start_x;
    int y_delta = (int)s_touch_last_y - (int)s_touch_start_y;
    int abs_x = x_delta < 0 ? -x_delta : x_delta;
    int abs_y = y_delta < 0 ? -y_delta : y_delta;

    event->x = s_touch_last_x;
    event->y = s_touch_last_y;
    if (abs_x >= TOUCH_SWIPE_THRESHOLD_PX && abs_x > abs_y) {
        event->gesture = x_delta < 0 ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;
    } else if (abs_y >= TOUCH_SWIPE_THRESHOLD_PX) {
        event->gesture = y_delta < 0 ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN;
    } else {
        event->gesture = TOUCH_TAP;
        event->x = s_touch_start_x;
        event->y = s_touch_start_y;
    }

    ESP_LOGI(TAG, "Touch %d at %u,%u", (int)event->gesture, event->x, event->y);
    return true;
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
    return BTN_NONE;
}

#endif
