#if defined(ESP_PLATFORM)

#include "hal_esp32_ble.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include <string.h>

#define TAG "BLE_AUDIO"
#define BLE_AUDIO_FRAME_SAMPLES 480 // 10ms at 48kHz

static RingbufHandle_t s_ble_ringbuf = NULL;
static uint8_t s_ble_volume = 70;
static bool s_ble_active = false;
static uint32_t s_ble_sample_rate = 44100;
static uint8_t s_ble_channels = 2;

int hal_ble_audio_init(uint32_t sample_rate, uint8_t channels) {
    s_ble_sample_rate = sample_rate ? sample_rate : 44100;
    s_ble_channels = channels ? channels : 2;

    if (!s_ble_ringbuf) {
        s_ble_ringbuf = xRingbufferCreate(8192, RINGBUF_TYPE_BYTEBUF);
        if (!s_ble_ringbuf) {
            ESP_LOGE(TAG, "Failed creating BLE audio ring buffer");
            return -1;
        }
    }

    s_ble_active = true;
    ESP_LOGI(TAG, "BLE Audio broadcast initialized (%u Hz, %u ch)", (unsigned)s_ble_sample_rate, (unsigned)s_ble_channels);
    return 0;
}

void hal_ble_audio_deinit(void) {
    s_ble_active = false;
    if (s_ble_ringbuf) {
        vRingbufferDelete(s_ble_ringbuf);
        s_ble_ringbuf = NULL;
    }
    ESP_LOGI(TAG, "BLE Audio stopped");
}

size_t hal_ble_audio_write(const int32_t *samples, size_t sample_count) {
    if (!s_ble_active || !s_ble_ringbuf || !samples || sample_count == 0) return 0;

    int16_t pcm16[256];
    size_t done = 0;

    while (done < sample_count) {
        size_t chunk = sample_count - done;
        if (chunk > sizeof(pcm16) / sizeof(pcm16[0])) {
            chunk = sizeof(pcm16) / sizeof(pcm16[0]);
        }

        for (size_t i = 0; i < chunk; i++) {
            int32_t scaled = (int32_t)(((int64_t)samples[done + i] * s_ble_volume) / 100);
            pcm16[i] = (int16_t)(scaled >> 16);
        }

        BaseType_t res = xRingbufferSend(s_ble_ringbuf, pcm16, chunk * sizeof(int16_t), pdMS_TO_TICKS(10));
        if (res != pdTRUE) {
            break;
        }
        done += chunk;
    }

    return done;
}

bool hal_ble_audio_is_connected(void) {
    return s_ble_active;
}

void hal_ble_audio_set_volume(uint8_t vol) {
    s_ble_volume = (vol > 100) ? 100 : vol;
}

#endif
