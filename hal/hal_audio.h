#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
} hal_audio_format_t;

// Initialize hardware audio sink (I2S DMA / DAC)
int hal_audio_init(uint32_t sample_rate, uint8_t channels);

// Write interleaved 32-bit signed samples (scaled to 32-bit)
// Returns number of samples consumed
size_t hal_audio_write(const int32_t *samples, size_t sample_count);

// Returns true if the audio queue has room and needs more samples
bool hal_audio_needs_data(void);

// Set volume percentage (0..100)
void hal_audio_set_volume(uint8_t volume);

// Pause or stop the audio stream
void hal_audio_stop(void);

// Resume audio stream
void hal_audio_resume(void);

// Deinitialize audio sink
void hal_audio_close(void);

#ifdef __cplusplus
}
#endif
