#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BTN_NONE = 0,
    BTN_PLAY_PAUSE,
    BTN_NEXT,
    BTN_PREV,
    BTN_VOL_UP,
    BTN_VOL_DOWN,
    BTN_BACK,
    BTN_SEEK_FWD,
    BTN_SEEK_BACK,
    BTN_CRASH_TEST
} btn_event_t;

typedef enum {
    TOUCH_NONE = 0,
    TOUCH_TAP,
    TOUCH_SWIPE_LEFT,
    TOUCH_SWIPE_RIGHT,
    TOUCH_SWIPE_UP,
    TOUCH_SWIPE_DOWN
} touch_gesture_t;

typedef struct {
    touch_gesture_t gesture;
    uint16_t x;
    uint16_t y;
} touch_event_t;

// Initialize button GPIOs and interrupts
void hal_input_init(void);

// Poll for next button event (non-blocking, returns BTN_NONE if empty)
btn_event_t hal_input_poll(void);

// Poll for a calibrated touch event. Coordinates use the display orientation.
bool hal_input_poll_touch(touch_event_t *event);

// CrowPanel diagnostic: sample XPT2046 pressure and raw axes on demand.
void hal_input_log_touch_probe(void);

#ifdef __cplusplus
}
#endif
