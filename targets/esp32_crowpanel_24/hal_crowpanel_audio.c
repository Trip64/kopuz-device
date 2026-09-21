#if defined(ESP_PLATFORM)

#include "hal/hal_audio.h"
#include "crowpanel_pins.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const char *TAG = "crow_audio";
static uint32_t s_sample_rate = 44100;
static uint8_t s_channels = 2;
static uint8_t s_volume = 70;
static bool s_running = false;

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    s_sample_rate = sample_rate ? sample_rate : 44100;
    s_channels = channels ? channels : 1;
    s_running = true;

    gpio_config_t speaker_config = {
        .pin_bit_mask = 1ULL << CROW_SPEAKER,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&speaker_config);
    gpio_set_level(CROW_SPEAKER, 0);

    ESP_LOGI(TAG, "UI-only audio sink: %lu Hz, %u channel(s)",
             (unsigned long)s_sample_rate, (unsigned)s_channels);
    return 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0 || !s_running) return 0;

    /* Keep playback timing honest until the on-board GPIO26 DAC path lands. */
    size_t frames = sample_count / (s_channels ? s_channels : 1);
    uint32_t delay_ms = (uint32_t)(((uint64_t)frames * 1000U) / s_sample_rate);
    if (delay_ms > 0) vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return sample_count;
}

bool hal_audio_needs_data(void) {
    return s_running;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = volume > 100 ? 100 : volume;
    (void)s_volume;
}

void hal_audio_stop(void) {
    s_running = false;
    gpio_set_level(CROW_SPEAKER, 0);
}

void hal_audio_resume(void) {
    s_running = true;
}

void hal_audio_close(void) {
    hal_audio_stop();
}

size_t hal_audio_write_stream(const uint8_t *data, size_t length) {
    (void)data;
    (void)length;
    return 0;
}

bool hal_audio_has_hardware_codec(void) {
    return false;
}

void hal_audio_beep(uint16_t frequency_hz, uint16_t duration_ms) {
    (void)frequency_hz;
    (void)duration_ms;
}

#endif
