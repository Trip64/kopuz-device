#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_power.h"
#include <stdbool.h>

int hal_battery_init(void) {
    return 0;
}

int hal_battery_read_mv(void) {
    return 3800; // Simulated LiPo 3.8V
}

int hal_ldr_init(void) {
    return 0;
}

int hal_ldr_read_raw(void) {
    return -1;
}

#endif
