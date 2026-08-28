#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_display.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// mikromedia Plus for STM32F7 Pin Mapping (SSD1963 16-bit Parallel TFT)
// Data Port: GPIOE (PE0..PE15) - 16-bit parallel bus
// Control Pins:
//   TFT_WR:   PF11 (Write Strobe)
//   TFT_RD:   PF12 (Read Strobe)
//   TFT_CS:   PF13 (Chip Select, Active Low)
//   TFT_RST:  PF14 (Reset, Active Low)
//   TFT_RS:   PF15 (Register Select / Data-Command)
//   TFT_BLED: PF10 (Backlight PWM Enable)

static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;
static uint8_t s_brightness = 100;

int hal_display_init(void) {
    // Initialize 16-bit parallel GPIO ports and SSD1963 controller
    return 0;
}

void hal_display_clear(void) {
}

void hal_display_flush(const uint8_t *mono_fb) {
    (void)mono_fb;
}

void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    (void)mono_fb; (void)x; (void)y; (void)w; (void)h;
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data) {
    (void)x; (void)y; (void)w; (void)h; (void)rgb565_data;
}

void hal_display_present(void) {
}

void hal_display_set_brightness(uint8_t pct) {
    s_brightness = (pct > 100) ? 100 : pct;
}

void hal_display_set_theme(uint16_t fg, uint16_t bg) {
    s_fg = fg;
    s_bg = bg;
}

void hal_display_sleep(void) {
}

#endif
