#if defined(ESP_PLATFORM)

#include "hal/hal_input.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#define PIN_BTN1 0   // Boot button (Select / Play-Pause)
#define PIN_BTN2 14  // Side button (Next)

static QueueHandle_t s_queue = NULL;
static int64_t s_press_time = 0;

void hal_input_init(void) {
    s_queue = xQueueCreate(8, sizeof(btn_event_t));

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN1) | (1ULL << PIN_BTN2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
}

btn_event_t hal_input_poll(void) {
    int64_t now = esp_timer_get_time() / 1000;

    // Button 1 (GPIO 0): Select / Play-Pause with long press for Back
    static bool s_btn1_was_down = false;
    bool btn1_down = (gpio_get_level(PIN_BTN1) == 0);

    if (btn1_down && !s_btn1_was_down) {
        s_press_time = now;
        s_btn1_was_down = true;
    } else if (!btn1_down && s_btn1_was_down) {
        s_btn1_was_down = false;
        int64_t held = now - s_press_time;
        if (held >= 500) {
            btn_event_t ev = BTN_BACK;
            xQueueSend(s_queue, &ev, 0);
        } else if (held >= 30) {
            btn_event_t ev = BTN_PLAY_PAUSE;
            xQueueSend(s_queue, &ev, 0);
        }
    }

    // Button 2 (GPIO 14): Next / Scroll
    static int64_t s_last_btn2 = 0;
    if (gpio_get_level(PIN_BTN2) == 0) {
        if (now - s_last_btn2 > 150) {
            s_last_btn2 = now;
            btn_event_t ev = BTN_NEXT;
            xQueueSend(s_queue, &ev, 0);
        }
    }

    btn_event_t ev = BTN_NONE;
    if (s_queue && xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
        return ev;
    }
    return BTN_NONE;
}

#endif
