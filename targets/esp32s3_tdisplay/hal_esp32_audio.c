#if defined(ESP_PLATFORM)

#include "hal/hal_audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define I2S_PIN_BCK   43
#define I2S_PIN_WS    44
#define I2S_PIN_DOUT  1

static i2s_chan_handle_t s_tx_chan = NULL;
static uint8_t s_volume = 70;
static uint32_t s_rate = 44100;
static bool s_running = false;

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    (void)channels;
    s_rate = sample_rate ? sample_rate : 44100;

    if (s_tx_chan) {
        i2s_channel_disable(s_tx_chan);
        i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(s_rate);
        i2s_channel_reconfig_std_clock(s_tx_chan, &clk);
        i2s_channel_enable(s_tx_chan);
        s_running = true;
        return 0;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 16;
    chan_cfg.dma_frame_num = 512;
    chan_cfg.auto_clear = true;
    i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);

    i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT; // 32 bits per channel (64 BCLK/frame) required by PCM5102A PLL!
    slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(s_rate),
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_PIN_BCK,
            .ws   = I2S_PIN_WS,
            .dout = I2S_PIN_DOUT,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    i2s_channel_enable(s_tx_chan);
    s_running = true;
    return 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (!s_tx_chan || !samples || sample_count == 0) return 0;
    if (!s_running) {
        i2s_channel_enable(s_tx_chan);
        s_running = true;
    }

    int16_t buf[512];
    size_t done = 0;

    while (done < sample_count) {
        size_t chunk = sample_count - done;
        if (chunk > sizeof(buf) / sizeof(buf[0])) chunk = sizeof(buf) / sizeof(buf[0]);

        for (size_t i = 0; i < chunk; i++) {
            int32_t s = samples[done + i] >> 16;
            int32_t scaled = (s * (int32_t)s_volume) / 100;
            if (scaled > 32767) scaled = 32767;
            if (scaled < -32768) scaled = -32768;
            buf[i] = (int16_t)scaled;
        }

        size_t wrote = 0;
        if (i2s_channel_write(s_tx_chan, buf, chunk * sizeof(int16_t), &wrote, portMAX_DELAY) != ESP_OK) {
            break;
        }
        done += chunk;
    }
    return done;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = volume > 100 ? 100 : volume;
}

void hal_audio_stop(void) {
    if (s_tx_chan && s_running) {
        i2s_channel_disable(s_tx_chan);
        s_running = false;
    }
}

void hal_audio_resume(void) {
    if (s_tx_chan && !s_running) {
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
}

bool hal_audio_needs_data(void) {
    return true;
}

#endif
