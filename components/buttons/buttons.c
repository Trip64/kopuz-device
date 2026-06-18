#include "buttons.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#define PIN_PLAY_PAUSE GPIO_NUM_4
#define PIN_NEXT       GPIO_NUM_5
#define PIN_PREV       GPIO_NUM_6
#define PIN_VOL_UP     GPIO_NUM_7
#define PIN_VOL_DOWN   GPIO_NUM_16

#define DEBOUNCE_US  (150 * 1000)
#define LONGPRESS_US (500 * 1000)

typedef struct {
    gpio_num_t pin;
    btn_event_t event;
    volatile int64_t last_us;
    volatile int64_t press_us;
} btn_t;

static btn_t s_btns[] = {
    {PIN_PLAY_PAUSE, BTN_PLAY_PAUSE, 0, 0},
    {PIN_NEXT, BTN_NEXT, 0, 0},
    {PIN_PREV, BTN_PREV, 0, 0},
    {PIN_VOL_UP, BTN_VOL_UP, 0, 0},
    {PIN_VOL_DOWN, BTN_VOL_DOWN, 0, 0},
};
#define NBTN (sizeof(s_btns) / sizeof(s_btns[0]))

static QueueHandle_t s_queue;
static bool s_inited;

static void IRAM_ATTR isr_handler(void *arg) {
    btn_t *b = (btn_t *)arg;
    int64_t now = esp_timer_get_time();

    if (b->pin == PIN_PLAY_PAUSE) {
        int level = gpio_get_level(b->pin);
        if (level == 0) {
            if (now - b->last_us < DEBOUNCE_US) return;
            b->last_us = now;
            b->press_us = now;
        } else {
            if (b->press_us == 0) return;
            int64_t held = now - b->press_us;
            b->press_us = 0;
            btn_event_t ev = (held >= LONGPRESS_US) ? BTN_BACK : BTN_PLAY_PAUSE;
            xQueueSendFromISR(s_queue, &ev, NULL);
        }
        return;
    }

    if (now - b->last_us < DEBOUNCE_US) return;
    b->last_us = now;
    btn_event_t ev = b->event;
    xQueueSendFromISR(s_queue, &ev, NULL);
}

void buttons_init(void) {
    if (s_inited) return;
    s_queue = xQueueCreate(8, sizeof(btn_event_t));

    gpio_install_isr_service(0);
    for (size_t i = 0; i < NBTN; i++) {
        gpio_config_t cfg = {
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .intr_type = (s_btns[i].pin == PIN_PLAY_PAUSE)
                             ? GPIO_INTR_ANYEDGE
                             : GPIO_INTR_NEGEDGE,
            .pin_bit_mask = 1ULL << s_btns[i].pin,
        };
        gpio_config(&cfg);
        gpio_isr_handler_add(s_btns[i].pin, isr_handler, &s_btns[i]);
    }
    s_inited = true;
}

btn_event_t buttons_poll(void) {
    btn_event_t ev = BTN_NONE;
    if (s_queue && xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
        return ev;
    }
    return BTN_NONE;
}
