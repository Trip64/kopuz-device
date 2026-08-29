#include "hal/hal_audio.h"
#include "sim_dac.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define SIM_DMA_DESC_NUM    12
#define SIM_DMA_FRAME_NUM   480
#define SIM_DMA_TOTAL_CAP   (SIM_DMA_DESC_NUM * SIM_DMA_FRAME_NUM) // 5760 frames

static SDL_AudioDeviceID s_dev = 0;
static uint32_t s_rate = 44100;
static uint8_t s_channels = 2;
static uint8_t s_volume = 70;
static bool s_running = false;

static sim_dac_model_t s_dac_model = SIM_DAC_PCM5102A_I2S;
static uint64_t s_total_samples = 0;
static uint32_t s_underrun_count = 0;
static uint32_t s_clip_count = 0;
static uint32_t s_clock_reconfigs = 0;
static float s_rms_l = -90.0f;
static float s_rms_r = -90.0f;

static const char* S_DAC_NAMES[SIM_DAC_MODEL_COUNT] = {
    [SIM_DAC_PCM5102A_I2S] = "TI PCM5102A (32-bit I2S Stereo)",
    [SIM_DAC_PT8211_LSBJ]  = "PT8211 (16-bit LSB-Justified)",
    [SIM_DAC_CS4344_I2S]   = "Cirrus Logic CS4344 (24-bit I2S)",
    [SIM_DAC_PWM_RING]     = "ESP32 Timer PWM Carrier (8-bit Mono)"
};

static const char* S_DAC_DESCS[SIM_DAC_MODEL_COUNT] = {
    [SIM_DAC_PCM5102A_I2S] = "32-bit I2S Philips slot | Auto-PLL master clock | BCK:GPIO43 WS:GPIO44 DOUT:GPIO1",
    [SIM_DAC_PT8211_LSBJ]  = "16-bit LSB-justified stereo | Dual DAC channels | Economy audio sink",
    [SIM_DAC_CS4344_I2S]   = "24-bit 192kHz multi-bit Delta-Sigma I2S DAC | High dynamic range",
    [SIM_DAC_PWM_RING]     = "Single-pin 40kHz PWM carrier | gptimer ISR ring buffer | Low-resource"
};

void hal_sim_dac_set_model(sim_dac_model_t model) {
    if (model < SIM_DAC_MODEL_COUNT) {
        s_dac_model = model;
        printf("[DAC_SIM] Switched DAC model to: %s\n", S_DAC_NAMES[s_dac_model]);
    }
}

sim_dac_model_t hal_sim_dac_get_model(void) {
    return s_dac_model;
}

const char* hal_sim_dac_get_model_name(sim_dac_model_t model) {
    if (model < SIM_DAC_MODEL_COUNT) return S_DAC_NAMES[model];
    return "Unknown DAC";
}

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    s_clock_reconfigs++;
    uint32_t target_rate = sample_rate ? sample_rate : 44100;
    uint8_t target_ch = channels ? channels : 2;

    if (s_dev != 0 && s_rate == target_rate && s_channels == target_ch) {
        SDL_ClearQueuedAudio(s_dev);
        SDL_PauseAudioDevice(s_dev, 0);
        s_running = true;
        return 0;
    }

    if (s_dev != 0) {
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
    }

    s_rate = target_rate;
    s_channels = target_ch;

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = (int)s_rate;
    want.format = AUDIO_S16SYS;
    want.channels = s_channels;
    want.samples = 1024;

    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_dev == 0) {
        printf("[DAC_SIM] SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_PauseAudioDevice(s_dev, 0);
    s_running = true;

    uint32_t bck_khz = (s_rate * (s_dac_model == SIM_DAC_PCM5102A_I2S ? 64 : 32)) / 1000;
    printf("[DAC_SIM] Hardware DAC initialized: %s\n", S_DAC_NAMES[s_dac_model]);
    printf("          Rate: %u Hz | Channels: %u | BCK: ~%u kHz | DMA Capacity: %u frames (%ums)\n",
           s_rate, s_channels, bck_khz, SIM_DMA_TOTAL_CAP, (SIM_DMA_TOTAL_CAP * 1000) / s_rate);

    return 0;
}

bool hal_audio_needs_data(void) {
    if (s_dev == 0 || !s_running) return false;
    uint32_t queued_bytes = SDL_GetQueuedAudioSize(s_dev);
    uint32_t target_queued = (s_rate * s_channels * sizeof(int16_t) * 120) / 1000; // ~120ms target
    if (queued_bytes == 0 && s_running) {
        s_underrun_count++;
    }
    return (queued_bytes < target_queued);
}

uint32_t hal_audio_get_queued_bytes(void) {
    return s_dev ? SDL_GetQueuedAudioSize(s_dev) : 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (s_dev == 0 || !samples || sample_count == 0) return 0;
    if (!s_running) {
        SDL_PauseAudioDevice(s_dev, 0);
        s_running = true;
    }

    int16_t s16_buf[1024];
    size_t done = 0;
    double sum_sq_l = 0;
    double sum_sq_r = 0;
    size_t count_l = 0;
    size_t count_r = 0;

    uint8_t bit_depth = (s_dac_model == SIM_DAC_PCM5102A_I2S) ? 32 :
                        ((s_dac_model == SIM_DAC_CS4344_I2S) ? 24 : 16);

    while (done < sample_count) {
        size_t chunk = sample_count - done;
        if (chunk > sizeof(s16_buf) / sizeof(s16_buf[0])) {
            chunk = sizeof(s16_buf) / sizeof(s16_buf[0]);
        }

        for (size_t i = 0; i < chunk; i++) {
            int32_t raw = samples[done + i];

            // Emulate DAC bit depth quantization
            if (bit_depth == 16) {
                raw &= (int32_t)0xFFFF0000;
            } else if (bit_depth == 24) {
                raw &= (int32_t)0xFFFFFF00;
            }

            int32_t s = raw >> 16;
            int32_t scaled = (s * (int32_t)s_volume) / 100;

            if (scaled > 32767) {
                scaled = 32767;
                s_clip_count++;
            } else if (scaled < -32768) {
                scaled = -32768;
                s_clip_count++;
            }

            s16_buf[i] = (int16_t)scaled;

            double norm = (double)scaled / 32768.0;
            if (s_channels == 1 || (i % 2 == 0)) {
                sum_sq_l += norm * norm;
                count_l++;
            } else {
                sum_sq_r += norm * norm;
                count_r++;
            }
        }

        SDL_QueueAudio(s_dev, s16_buf, (Uint32)(chunk * sizeof(int16_t)));
        done += chunk;
    }

    s_total_samples += sample_count;

    if (count_l > 0) {
        double rms_l = sqrt(sum_sq_l / (double)count_l);
        s_rms_l = (rms_l > 0.00001) ? (float)(20.0 * log10(rms_l)) : -90.0f;
    }
    if (count_r > 0) {
        double rms_r = sqrt(sum_sq_r / (double)count_r);
        s_rms_r = (rms_r > 0.00001) ? (float)(20.0 * log10(rms_r)) : -90.0f;
    } else {
        s_rms_r = s_rms_l;
    }

    return sample_count;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = volume > 100 ? 100 : volume;
}

void hal_audio_stop(void) {
    if (s_dev != 0) {
        SDL_ClearQueuedAudio(s_dev);
        SDL_PauseAudioDevice(s_dev, 1);
        s_running = false;
    }
}

void hal_audio_resume(void) {
    if (s_dev != 0) {
        SDL_PauseAudioDevice(s_dev, 0);
        s_running = true;
    }
}

void hal_audio_close(void) {
    if (s_dev != 0) {
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
        s_running = false;
    }
}

void hal_audio_beep(uint16_t freq_hz, uint16_t duration_ms) {
    (void)freq_hz;
    (void)duration_ms;
}

bool hal_audio_has_hardware_codec(void) {
    return false;
}

size_t hal_audio_write_stream(const uint8_t *data, size_t len) {
    (void)data;
    (void)len;
    return 0;
}

void hal_sim_dac_get_status(sim_dac_status_t *st) {
    if (!st) return;
    st->model = s_dac_model;
    st->name = S_DAC_NAMES[s_dac_model];
    st->desc = S_DAC_DESCS[s_dac_model];
    st->sample_rate = s_rate;
    st->channels = s_channels;
    st->bit_depth = (s_dac_model == SIM_DAC_PCM5102A_I2S) ? 32 :
                    ((s_dac_model == SIM_DAC_CS4344_I2S) ? 24 : 16);
    st->volume = s_volume;
    st->is_running = s_running;
    st->is_muted = (s_volume == 0);
    st->dma_capacity_frames = SIM_DMA_TOTAL_CAP;

    uint32_t queued_bytes = s_dev ? SDL_GetQueuedAudioSize(s_dev) : 0;
    uint32_t bytes_per_frame = s_channels * sizeof(int16_t);
    st->dma_queued_frames = bytes_per_frame ? (queued_bytes / bytes_per_frame) : 0;
    st->dma_fill_pct = (float)st->dma_queued_frames / (float)SIM_DMA_TOTAL_CAP * 100.0f;
    if (st->dma_fill_pct > 100.0f) st->dma_fill_pct = 100.0f;

    st->rms_db_l = s_rms_l;
    st->rms_db_r = s_rms_r;
    st->total_samples_played = s_total_samples;
    st->underrun_count = s_underrun_count;
    st->clip_count = s_clip_count;
    st->clock_reconfigs = s_clock_reconfigs;
    st->pin_bck = "GPIO 43";
    st->pin_ws = "GPIO 44";
    st->pin_dout = "GPIO 1";
}

void hal_sim_dac_reset_counters(void) {
    s_underrun_count = 0;
    s_clip_count = 0;
}

void hal_sim_dac_print_status(void) {
    sim_dac_status_t st;
    hal_sim_dac_get_status(&st);
    printf("=========================================================================================\n");
    printf("                            Hardware DAC Simulation Status                               \n");
    printf("=========================================================================================\n");
    printf("  Chip Model     : %s\n", st.name);
    printf("  Hardware Bus   : %s\n", st.desc);
    printf("  Audio Clocks   : Rate=%u Hz | Ch=%u (%s) | Slot Depth=%u-bit\n",
           st.sample_rate, st.channels, st.channels == 2 ? "Stereo" : "Mono", st.bit_depth);
    printf("  I2S Pinout     : BCK=%s | WS=%s | DOUT=%s\n", st.pin_bck, st.pin_ws, st.pin_dout);
    printf("  DMA Buffer     : %u / %u frames (%.1f%% filled) | State: %s\n",
           st.dma_queued_frames, st.dma_capacity_frames, st.dma_fill_pct,
           st.is_running ? "STREAMING" : "IDLE");
    printf("  Signal Levels  : Left: %6.1f dBFS | Right: %6.1f dBFS | Vol: %u%%\n",
           st.rms_db_l, st.rms_db_r, st.volume);
    printf("  Health Check   : Underruns=%u | Clips=%u | Clock Reconfigs=%u\n",
           st.underrun_count, st.clip_count, st.clock_reconfigs);
    printf("=========================================================================================\n");
}
