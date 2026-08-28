#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DISP_TECH_COLOR_RGB565 = 0,
    DISP_TECH_OLED_BLUE_MONO,
    DISP_TECH_OLED_WHITE_MONO,
    DISP_TECH_SHARP_MIP_MONO,
    DISP_TECH_EPAPER_BWR
} sim_disp_tech_t;

typedef struct {
    const char *id;
    const char *name;
    const char *desc;
    uint16_t width;
    uint16_t height;
    uint8_t scale;
    sim_disp_tech_t tech;
    uint16_t fixed_fg;
    uint16_t fixed_bg;
    uint16_t fixed_accent;
    uint8_t art_size;
    uint16_t bus_delay_ms;
} sim_display_profile_t;

uint8_t hal_sim_display_get_mode(void);
void hal_sim_display_set_mode(uint8_t mode_idx);
const char* hal_sim_display_get_mode_name(uint8_t mode_idx);
const char* hal_sim_display_get_mode_id(uint8_t mode_idx);
const char* hal_sim_display_get_mode_desc(uint8_t mode_idx);
uint8_t hal_sim_display_get_mode_count(void);
void hal_sim_display_get_size(uint16_t *w, uint16_t *h);
bool hal_sim_display_is_color(void);
bool hal_sim_display_supports_themes(void);
uint8_t hal_sim_display_get_art_size(void);
void hal_sim_display_set_scale_override(uint8_t scale);
void hal_sim_display_blit_art(int16_t dst_x, int16_t dst_y, uint8_t dst_w, uint8_t dst_h,
                             const uint8_t *src_rgb565, uint8_t src_w, uint8_t src_h);

