#include "audio_out.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_out";
static volatile uint8_t s_volume = 70;
static uint32_t s_sample_rate;
static uint8_t s_channels;

#define AUDIO_BACKEND_I2S 1

#if defined(AUDIO_BACKEND_I2S)
#include "driver/i2s_std.h"

#define I2S_PIN_BCK   GPIO_NUM_47
#define I2S_PIN_WS    GPIO_NUM_18
#define I2S_PIN_DOUT  GPIO_NUM_21

#define I2S_CHUNK_FRAMES 256

static i2s_chan_handle_t s_tx;
static bool s_inited;
static bool s_running;

static esp_err_t i2s_apply_clock(uint32_t sample_rate) {
    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    return i2s_channel_reconfig_std_clock(s_tx, &clk);
}

int audio_out_init(uint32_t sample_rate, uint8_t channels) {
    s_sample_rate = sample_rate ? sample_rate : 44100;
    s_channels = channels ? channels : 1;

    if (s_inited) {
        if (s_running) {
            i2s_channel_disable(s_tx);
            s_running = false;
        }
        esp_err_t e = i2s_apply_clock(s_sample_rate);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "reconfig clock: %s", esp_err_to_name(e));
            return -1;
        }
        i2s_channel_enable(s_tx);
        s_running = true;
        ESP_LOGI(TAG, "retuned to %u Hz, %u ch", s_sample_rate, s_channels);
        return 0;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 16;
    chan_cfg.dma_frame_num = 480;
    esp_err_t e;
    if ((e = i2s_new_channel(&chan_cfg, &s_tx, NULL)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(e));
        return -1;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(s_sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_STEREO),
    };
    std_cfg.gpio_cfg = (i2s_std_gpio_config_t){
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_PIN_BCK,
            .ws   = I2S_PIN_WS,
            .dout = I2S_PIN_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
    };
    if ((e = i2s_channel_init_std_mode(s_tx, &std_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode: %s", esp_err_to_name(e));
        return -1;
    }
    if ((e = i2s_channel_enable(s_tx)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable: %s", esp_err_to_name(e));
        return -1;
    }

    s_inited = true;
    s_running = true;
    ESP_LOGI(TAG, "I2S backend up: BCK=%d WS=%d DOUT=%d, %u Hz, %u ch -> PCM5102A",
             I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, s_sample_rate, s_channels);
    return 0;
}

size_t audio_out_write(const int16_t *samples, size_t sample_count) {
    if (!s_inited) return 0;
    if (!s_running) {
        i2s_channel_enable(s_tx);
        s_running = true;
    }

    uint8_t ch = s_channels ? s_channels : 1;
    size_t frames = sample_count / ch;
    int32_t scratch[I2S_CHUNK_FRAMES * 2];
    size_t done = 0;

    while (done < frames) {
        size_t n = frames - done;
        if (n > I2S_CHUNK_FRAMES) n = I2S_CHUNK_FRAMES;

        for (size_t i = 0; i < n; i++) {
            const int16_t *src = &samples[(done + i) * ch];
            int32_t l = ((int32_t)src[0] * s_volume) / 100;
            int32_t r = (ch > 1)
                            ? ((int32_t)src[1] * s_volume) / 100
                            : l;
            scratch[i * 2]     = l << 16;
            scratch[i * 2 + 1] = r << 16;
        }

        size_t wrote = 0;
        if (i2s_channel_write(s_tx, scratch, n * 2 * sizeof(int32_t),
                              &wrote, portMAX_DELAY) != ESP_OK) {
            break;
        }
        done += n;
    }
    return done * ch;
}

void audio_out_stop(void) {
    if (s_inited && s_running) {
        i2s_channel_disable(s_tx);
        s_running = false;
    }
}

#elif defined(AUDIO_BACKEND_PWM)
#include "driver/ledc.h"
#include "driver/gptimer.h"
#include "hal/ledc_ll.h"

#define PWM_PIN        GPIO_NUM_15
#define PWM_TIMER      LEDC_TIMER_0
#define PWM_CHANNEL    LEDC_CHANNEL_0
#define PWM_MODE       LEDC_LOW_SPEED_MODE
#define PWM_RES        LEDC_TIMER_8_BIT
#define PWM_FREQ_HZ    40000

#define PWM_SILENCE    128

#define VOLUME_BOOST_X 4

#define RING_SIZE  8192u
#define RING_MASK  (RING_SIZE - 1u)
static volatile uint8_t s_ring[RING_SIZE];
static volatile uint32_t s_head;
static volatile uint32_t s_tail;

static gptimer_handle_t s_timer;
static ledc_dev_t *s_hw;
static bool s_inited;

static inline void IRAM_ATTR pwm_set_duty(uint32_t duty) {
    ledc_ll_set_duty_int_part(s_hw, PWM_MODE, PWM_CHANNEL, duty);
    ledc_ll_set_duty_start(s_hw, PWM_MODE, PWM_CHANNEL, true);
    ledc_ll_ls_channel_update(s_hw, PWM_MODE, PWM_CHANNEL);
}

static bool IRAM_ATTR on_sample(gptimer_handle_t t,
                                const gptimer_alarm_event_data_t *e,
                                void *arg) {
    uint32_t tail = s_tail;
    if (tail != s_head) {
        pwm_set_duty(s_ring[tail]);
        s_tail = (tail + 1) & RING_MASK;
    } else {
        pwm_set_duty(PWM_SILENCE);
    }
    return false;
}

int audio_out_init(uint32_t sample_rate, uint8_t channels) {
    s_sample_rate = sample_rate ? sample_rate : 44100;
    s_channels = channels ? channels : 1;

    if (s_inited) {
        s_head = s_tail = 0;
        gptimer_alarm_config_t alarm = {
            .alarm_count = 1000000ULL / s_sample_rate,
            .reload_count = 0,
            .flags.auto_reload_on_alarm = true,
        };
        gptimer_set_alarm_action(s_timer, &alarm);
        ESP_LOGI(TAG, "retuned to %u Hz, %u ch", s_sample_rate, s_channels);
        return 0;
    }

    ledc_timer_config_t tcfg = {
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .duty_resolution = PWM_RES,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tcfg);

    ledc_channel_config_t ccfg = {
        .speed_mode = PWM_MODE,
        .channel = PWM_CHANNEL,
        .timer_sel = PWM_TIMER,
        .gpio_num = PWM_PIN,
        .duty = PWM_SILENCE,
        .hpoint = 0,
    };
    ledc_channel_config(&ccfg);

    s_hw = LEDC_LL_GET_HW();
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, PWM_SILENCE);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);

    s_head = s_tail = 0;
    gptimer_config_t gcfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    esp_err_t e;
    if ((e = gptimer_new_timer(&gcfg, &s_timer)) != ESP_OK) {
        ESP_LOGE(TAG, "gptimer_new_timer: %s", esp_err_to_name(e));
        return -1;
    }
    gptimer_event_callbacks_t cbs = { .on_alarm = on_sample };
    if ((e = gptimer_register_event_callbacks(s_timer, &cbs, NULL)) != ESP_OK) {
        ESP_LOGE(TAG, "gptimer_register_event_callbacks: %s", esp_err_to_name(e));
        return -1;
    }
    gptimer_alarm_config_t alarm = {
        .alarm_count = 1000000ULL / s_sample_rate,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    if ((e = gptimer_set_alarm_action(s_timer, &alarm)) != ESP_OK) {
        ESP_LOGE(TAG, "gptimer_set_alarm_action: %s", esp_err_to_name(e));
        return -1;
    }
    if ((e = gptimer_enable(s_timer)) != ESP_OK) {
        ESP_LOGE(TAG, "gptimer_enable: %s", esp_err_to_name(e));
        return -1;
    }
    if ((e = gptimer_start(s_timer)) != ESP_OK) {
        ESP_LOGE(TAG, "gptimer_start: %s", esp_err_to_name(e));
        return -1;
    }

    s_inited = true;
    ESP_LOGI(TAG, "PWM backend up: GPIO%d, %u Hz carrier, %u Hz sample, "
                  "alarm=%llu ticks, %u ch (gptimer ISR + ring)",
             PWM_PIN, (unsigned)PWM_FREQ_HZ, s_sample_rate,
             (unsigned long long)(1000000ULL / s_sample_rate), s_channels);
    return 0;
}

size_t audio_out_write(const int16_t *samples, size_t sample_count) {
    uint8_t ch = s_channels ? s_channels : 1;
    size_t frames = sample_count / ch;
    for (size_t i = 0; i < frames; i++) {
        int32_t s = samples[i * ch];
        if (ch > 1) {
            s = (s + samples[i * ch + 1]) / 2;
        }
        s = (s * (int32_t)s_volume * VOLUME_BOOST_X) / 100;
        if (s > 32767) s = 32767;
        else if (s < -32768) s = -32768;
        uint8_t pwm = (uint8_t)((s + 32768) >> 8);

        uint32_t next = (s_head + 1) & RING_MASK;
        while (next == s_tail) {
            vTaskDelay(1);
        }
        s_ring[s_head] = pwm;
        s_head = next;
    }
    return frames * ch;
}

void audio_out_stop(void) {
    s_head = s_tail = 0;
    if (s_hw) pwm_set_duty(PWM_SILENCE);
}

#endif

void audio_out_set_volume(uint8_t volume) {
    s_volume = volume > 100 ? 100 : volume;
}
