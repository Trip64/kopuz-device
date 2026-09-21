#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

int hal_ble_audio_init(uint32_t sample_rate, uint8_t channels);
void hal_ble_audio_deinit(void);
size_t hal_ble_audio_write(const int32_t *samples, size_t sample_count);
bool hal_ble_audio_is_connected(void);
void hal_ble_audio_set_volume(uint8_t volume);
const char *hal_ble_audio_get_device_name(void);
void hal_ble_audio_start_scan(void);
void hal_ble_audio_stop_scan(void);
bool hal_ble_audio_is_scanning(void);
uint8_t hal_ble_audio_get_discovered(bt_device_entry_t *devices, uint8_t max_count);
bool hal_ble_audio_connect_device(uint8_t index);
void hal_ble_audio_disconnect(void);

#ifdef __cplusplus
}
#endif
