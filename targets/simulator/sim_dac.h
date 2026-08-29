#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SIM_DAC_PCM5102A_I2S = 0,
    SIM_DAC_PT8211_LSBJ,
    SIM_DAC_CS4344_I2S,
    SIM_DAC_PWM_RING,
    SIM_DAC_MODEL_COUNT
} sim_dac_model_t;

typedef struct {
    sim_dac_model_t model;
    const char *name;
    const char *desc;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bit_depth;
    uint8_t volume;
    bool is_running;
    bool is_muted;
    uint32_t dma_capacity_frames;
    uint32_t dma_queued_frames;
    float dma_fill_pct;
    float rms_db_l;
    float rms_db_r;
    uint64_t total_samples_played;
    uint32_t underrun_count;
    uint32_t clip_count;
    uint32_t clock_reconfigs;
    const char *pin_bck;
    const char *pin_ws;
    const char *pin_dout;
} sim_dac_status_t;

void hal_sim_dac_set_model(sim_dac_model_t model);
sim_dac_model_t hal_sim_dac_get_model(void);
const char* hal_sim_dac_get_model_name(sim_dac_model_t model);
void hal_sim_dac_get_status(sim_dac_status_t *out_status);
void hal_sim_dac_print_status(void);
void hal_sim_dac_reset_counters(void);
