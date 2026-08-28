#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_input.h"
#include <stdbool.h>

void hal_input_init(void) {
}

btn_event_t hal_input_poll(void) {
    return BTN_NONE;
}

#endif
