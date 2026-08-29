#if defined(ESP_PLATFORM)

#include "hal/hal_audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define TAG "AUDIO_I2S"

#define I2S_PIN_BCK   43
#define I2S_PIN_WS    44
#define I2S_PIN_DOUT  1

#define I2S_CHUNK_FRAMES 256

static i2s_chan_handle_t s_tx_chan = NULL;
static uint8_t s_volume = 70;
static uint32_t s_sample_rate = 44100;
static uint8_t s_channels = 2;
static bool s_inited = false;
static bool s_running = false;

static esp_err_t i2s_apply_clock(uint32_t sample_rate) {
    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    return i2s_channel_reconfig_std_clock(s_tx_chan, &clk);
}

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    s_sample_rate = sample_rate ? sample_rate : 44100;
    s_channels = channels ? channels : 2;

    if (s_inited) {
        if (s_running) {
            i2s_channel_disable(s_tx_chan);
            s_running = false;
        }
        esp_err_t e = i2s_apply_clock(s_sample_rate);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "reconfig clock error: %s", esp_err_to_name(e));
            return -1;
        }
        i2s_channel_enable(s_tx_chan);
        s_running = true;
        ESP_LOGI(TAG, "Reconfigured I2S to %u Hz, %u ch", (unsigned)s_sample_rate, (unsigned)s_channels);
        return 0;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 12;
    chan_cfg.dma_frame_num = 480;
    chan_cfg.auto_clear = true;

    esp_err_t e;
    if ((e = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(e));
        return -1;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(s_sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_PIN_BCK,
            .ws   = I2S_PIN_WS,
            .dout = I2S_PIN_DOUT,
            .din  = I2S_GPIO_UNUSED,
        },
    };

    if ((e = i2s_channel_init_std_mode(s_tx_chan, &std_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode: %s", esp_err_to_name(e));
        return -1;
    }
    if ((e = i2s_channel_enable(s_tx_chan)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable: %s", esp_err_to_name(e));
        return -1;
    }

    s_inited = true;
    s_running = true;
    ESP_LOGI(TAG, "I2S backend up: BCK=%d WS=%d DOUT=%d, %u Hz, %u ch -> PCM5102A",
             I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, (unsigned)s_sample_rate, (unsigned)s_channels);
    return 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (!s_inited || !s_tx_chan || !samples || sample_count == 0) return 0;
    if (!s_running) {
        i2s_channel_enable(s_tx_chan);
        s_running = true;
    }

    uint8_t ch = s_channels ? s_channels : 2;
    size_t frames = sample_count / ch;
    int32_t scratch[I2S_CHUNK_FRAMES * 2];
    size_t done = 0;

    while (done < frames) {
        size_t n = frames - done;
        if (n > I2S_CHUNK_FRAMES) n = I2S_CHUNK_FRAMES;

        for (size_t i = 0; i < n; i++) {
            const int32_t *src = &samples[(done + i) * ch];
            int32_t l = (int32_t)(((int64_t)src[0] * s_volume) / 100);
            int32_t r = (ch > 1) ? (int32_t)(((int64_t)src[1] * s_volume) / 100) : l;
            scratch[i * 2]     = l;
            scratch[i * 2 + 1] = r;
        }

        size_t wrote = 0;
        if (i2s_channel_write(s_tx_chan, scratch, n * 2 * sizeof(int32_t), &wrote, portMAX_DELAY) != ESP_OK) {
            break;
        }
        done += n;
    }
    return done * ch;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = volume > 100 ? 100 : volume;
}

void hal_audio_stop(void) {
    if (s_inited && s_running) {
        i2s_channel_disable(s_tx_chan);
        s_running = false;
    }
}

void hal_audio_resume(void) {
    if (s_inited && !s_running) {
        i2s_channel_enable(s_tx_chan);
        s_running = true;
    }
}

void hal_audio_close(void) {
    hal_audio_stop();
    if (s_tx_chan) {
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
    }
    s_inited = false;
    s_running = false;
}

bool hal_audio_needs_data(void) {
    return true;
}

#endif
