#include "hal/hal_power.h"

int hal_battery_init(void) {
    return 0;
}

int hal_battery_read_mv(void) {
    return 3950;
}

int hal_ldr_init(void) {
    return 0;
}

int hal_ldr_read_raw(void) {
    return 1500;
}
