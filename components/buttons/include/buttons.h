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
} btn_event_t;

void buttons_init(void);

btn_event_t buttons_poll(void);

#ifdef __cplusplus
}
#endif
