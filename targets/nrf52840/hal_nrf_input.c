#if defined(NRF52840_XXAA) || defined(TARGET_NRF52840)

#include "hal/hal_input.h"
#include <stdbool.h>

void hal_input_init(void) {
}

btn_event_t hal_input_poll(void) {
    return BTN_NONE;
}

#endif
