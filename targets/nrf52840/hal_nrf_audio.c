#if defined(NRF52840_XXAA) || defined(TARGET_NRF52840)

#include "hal/hal_audio.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define I2S_DMA_BUFFER_SIZE  512

static int32_t s_dma_buffer_0[I2S_DMA_BUFFER_SIZE * 2];
static int32_t s_dma_buffer_1[I2S_DMA_BUFFER_SIZE * 2];
static uint8_t s_active_buf = 0;
static uint8_t s_volume = 70;
static uint32_t s_sample_rate = 44100;
static uint8_t s_channels = 2;
static bool s_running = false;

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    s_sample_rate = sample_rate ? sample_rate : 44100;
    s_channels = channels ? channels : 2;
    s_running = true;
    return 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0 || !s_running) return 0;

    int32_t *dest = (s_active_buf == 0) ? s_dma_buffer_0 : s_dma_buffer_1;
    size_t chunk = sample_count;
    if (chunk > sizeof(s_dma_buffer_0) / sizeof(s_dma_buffer_0[0])) {
        chunk = sizeof(s_dma_buffer_0) / sizeof(s_dma_buffer_0[0]);
    }

    for (size_t i = 0; i < chunk; i++) {
        dest[i] = (int32_t)(((int64_t)samples[i] * s_volume) / 100);
    }

    s_active_buf ^= 1;
    return chunk;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = (volume > 100) ? 100 : volume;
}

void hal_audio_stop(void) {
    s_running = false;
}

void hal_audio_resume(void) {
    s_running = true;
}

bool hal_audio_needs_data(void) {
    return s_running;
}

void hal_audio_close(void) {
    s_running = false;
}

bool hal_audio_has_hardware_codec(void) {
    return false;
}

size_t hal_audio_write_stream(const uint8_t *data, size_t len) {
    (void)data;
    (void)len;
    return 0;
}

void hal_audio_beep(uint16_t freq_hz, uint16_t duration_ms) {
    (void)freq_hz;
    (void)duration_ms;
}

#endif

