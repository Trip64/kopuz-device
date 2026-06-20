#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int audio_out_init(uint32_t sample_rate, uint8_t channels);

// `sample_count` = number of interleaved int32 samples in `samples` (length of
// the slice, NOT frames). Samples are full-scale signed 32-bit; channel count
// comes from audio_out_init. Returns the number of samples consumed.
size_t audio_out_write(const int32_t *samples, size_t sample_count);

void audio_out_set_volume(uint8_t volume);

void audio_out_stop(void);

#ifdef __cplusplus
}
#endif
