#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Bluetooth Audio source (A2DP / BLE Audio)
int hal_ble_audio_init(uint32_t sample_rate, uint8_t channels);

// Deinitialize Bluetooth Audio
void hal_ble_audio_deinit(void);

// Write interleaved 32-bit audio samples to Bluetooth stream
size_t hal_ble_audio_write(const int32_t *samples, size_t sample_count);

// Returns true if a Bluetooth Audio sink (headphones/speaker) is currently connected
bool hal_ble_audio_is_connected(void);

// Set Bluetooth audio streaming volume (0..100)
void hal_ble_audio_set_volume(uint8_t vol);

// Get the name of the currently connected Bluetooth device (or "Searching...")
const char* hal_ble_audio_get_device_name(void);

#ifdef __cplusplus
}
#endif
