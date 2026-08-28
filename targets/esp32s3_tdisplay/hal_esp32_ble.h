#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int hal_ble_audio_init(uint32_t sample_rate, uint8_t channels);
void hal_ble_audio_deinit(void);
size_t hal_ble_audio_write(const int32_t *samples, size_t sample_count);
bool hal_ble_audio_is_connected(void);
void hal_ble_audio_set_volume(uint8_t vol);

#ifdef __cplusplus
}
#endif
