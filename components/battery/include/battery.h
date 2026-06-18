#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Configure the battery-voltage ADC. Idempotent. Returns 0 on success.
int battery_init(void);

/// Battery voltage in millivolts (after the on-board divider), averaged over a
/// few samples. Returns a negative value on error.
int battery_read_mv(void);

#ifdef __cplusplus
}
#endif
