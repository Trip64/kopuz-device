#pragma once

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

// Initialize button GPIOs and interrupts
void hal_input_init(void);

// Poll for next button event (non-blocking, returns BTN_NONE if empty)
btn_event_t hal_input_poll(void);

#ifdef __cplusplus
}
#endif
