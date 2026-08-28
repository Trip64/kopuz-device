#if defined(NRF52840_XXAA) || defined(TARGET_NRF52840)

#include "hal/hal_power.h"
#include <stdbool.h>

int hal_battery_init(void) {
    return 0;
}

int hal_battery_read_mv(void) {
    return 3800;
}

int hal_ldr_init(void) {
    return 0;
}

int hal_ldr_read_raw(void) {
    return -1;
}

#endif
