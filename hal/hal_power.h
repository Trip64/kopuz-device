#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize battery ADC reading
int hal_battery_init(void);

// Read battery voltage in millivolts (e.g. 3700 for 3.7V), or negative if USB / not connected
int hal_battery_read_mv(void);

// Optional ambient light sensor initialization
int hal_ldr_init(void);

// Read raw ADC light reading (0..4095), or negative if not present
int hal_ldr_read_raw(void);

#ifdef __cplusplus
}
#endif
