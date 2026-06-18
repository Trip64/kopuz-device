#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int audio_out_init(uint32_t sample_rate, uint8_t channels);

size_t audio_out_write(const int16_t *samples, size_t frames);

void audio_out_set_volume(uint8_t volume);

void audio_out_stop(void);

#ifdef __cplusplus
}
#endif
