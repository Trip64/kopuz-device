#include "hal/hal_display.h"
#include "config.h"

#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"

// ILI9341 SPI0 Pin Configuration on RP2040 / RP2350
#define PICO_LCD_SPI     spi0
#define PIN_LCD_MISO     16
#define PIN_LCD_CS       17
#define PIN_LCD_SCLK     18
#define PIN_LCD_MOSI     19
#define PIN_LCD_DC       20
#define PIN_LCD_RST      21
#define PIN_LCD_BL       22

static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;

static void spi_write(const uint8_t *data, size_t len) {
    if (len == 0) return;
    spi_write_blocking(PICO_LCD_SPI, data, len);
}

static void send_command(uint8_t cmd) {
    gpio_put(PIN_LCD_DC, 0);
    gpio_put(PIN_LCD_CS, 0);
    spi_write(&cmd, 1);
    gpio_put(PIN_LCD_CS, 1);
}

static void send_data_buf(const uint8_t *buf, size_t len) {
    if (len == 0) return;
    gpio_put(PIN_LCD_DC, 1);
    gpio_put(PIN_LCD_CS, 0);
    spi_write(buf, len);
    gpio_put(PIN_LCD_CS, 1);
}

static void send_data(uint8_t d) {
    send_data_buf(&d, 1);
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t cax[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    uint8_t pax[4] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    send_command(0x2A);
    send_data_buf(cax, 4);
    send_command(0x2B);
    send_data_buf(pax, 4);
    send_command(0x2C);
}

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[16];
} ili_init_cmd_t;

static const ili_init_cmd_t INIT_SEQ[] = {
    {0xEF, 3, {0x03, 0x80, 0x02}},
    {0xCF, 3, {0x00, 0xC1, 0x30}},
    {0xED, 4, {0x64, 0x03, 0x12, 0x81}},
    {0xE8, 3, {0x85, 0x00, 0x78}},
    {0xCB, 5, {0x39, 0x2C, 0x00, 0x34, 0x02}},
    {0xF7, 1, {0x20}},
    {0xEA, 2, {0x00, 0x00}},
    {0xC0, 1, {0x23}},
    {0xC1, 1, {0x10}},
    {0xC5, 2, {0x3E, 0x28}},
    {0xC7, 1, {0x86}},
    {0x36, 1, {0x28}}, // MADCTL 0x28: 320x240 landscape (MV set, BGR)
    {0x3A, 1, {0x55}}, // 16-bit RGB565
    {0xB1, 2, {0x00, 0x18}},
    {0xB6, 3, {0x08, 0x82, 0x27}},
    {0xF2, 1, {0x00}},
    {0x26, 1, {0x01}},
    {0xE0, 15, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07,
                0x10, 0x03, 0x0E, 0x09, 0x00}},
    {0xE1, 15, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08,
                0x0F, 0x0C, 0x31, 0x36, 0x0F}},
};
#define INIT_SEQ_LEN (sizeof(INIT_SEQ) / sizeof(INIT_SEQ[0]))

int hal_display_init(void) {
    spi_init(PICO_LCD_SPI, 40 * 1000 * 1000);
    gpio_set_function(PIN_LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_SCLK, GPIO_FUNC_SPI);

    gpio_init(PIN_LCD_CS);  gpio_set_dir(PIN_LCD_CS, GPIO_OUT);  gpio_put(PIN_LCD_CS, 1);
    gpio_init(PIN_LCD_DC);  gpio_set_dir(PIN_LCD_DC, GPIO_OUT);  gpio_put(PIN_LCD_DC, 1);
    gpio_init(PIN_LCD_RST); gpio_set_dir(PIN_LCD_RST, GPIO_OUT);

    gpio_put(PIN_LCD_RST, 1); sleep_ms(10);
    gpio_put(PIN_LCD_RST, 0); sleep_ms(20);
    gpio_put(PIN_LCD_RST, 1); sleep_ms(150);

    for (size_t i = 0; i < INIT_SEQ_LEN; i++) {
        send_command(INIT_SEQ[i].cmd);
        if (INIT_SEQ[i].len > 0) {
            send_data_buf(INIT_SEQ[i].data, INIT_SEQ[i].len);
        }
    }

    send_command(0x11);
    sleep_ms(120);
    send_command(0x29);
    sleep_ms(20);

    gpio_set_function(PIN_LCD_BL, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_LCD_BL);
    pwm_set_wrap(slice, 255);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_LCD_BL), 255);
    pwm_set_enabled(slice, true);

    hal_display_clear();
    return 0;
}

void hal_display_set_brightness(uint8_t pct) {
    uint slice = pwm_gpio_to_slice_num(PIN_LCD_BL);
    uint16_t duty = (pct > 100 ? 100 : pct) * 255 / 100;
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_LCD_BL), duty);
}

void hal_display_set_theme(uint16_t fg, uint16_t bg) {
    s_fg = fg;
    s_bg = bg;
}

void hal_display_clear(void) {
    set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    uint8_t line[LCD_WIDTH * 2];
    for (int i = 0; i < LCD_WIDTH; i++) {
        line[i * 2]     = (uint8_t)(s_bg >> 8);
        line[i * 2 + 1] = (uint8_t)(s_bg & 0xFF);
    }
    for (int y = 0; y < LCD_HEIGHT; y++) {
        send_data_buf(line, sizeof(line));
    }
}

void hal_display_flush(const uint8_t *mono_fb) {
    if (!mono_fb) return;
    set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    uint8_t line[LCD_WIDTH * 2];
    uint8_t bg_h = (uint8_t)(s_bg >> 8), bg_l = (uint8_t)(s_bg & 0xFF);
    uint8_t fg_h = (uint8_t)(s_fg >> 8), fg_l = (uint8_t)(s_fg & 0xFF);
    uint16_t row_bytes = LCD_ROW_BYTES_1BPP;

    for (int y = 0; y < LCD_HEIGHT; y++) {
        const uint8_t *row = &mono_fb[y * row_bytes];
        for (int x = 0; x < LCD_WIDTH; x++) {
            int bit = (row[x >> 3] >> (7 - (x & 7))) & 1;
            if (bit) {
                line[x * 2]     = bg_h;
                line[x * 2 + 1] = bg_l;
            } else {
                line[x * 2]     = fg_h;
                line[x * 2 + 1] = fg_l;
            }
        }
        send_data_buf(line, sizeof(line));
    }
}

void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!mono_fb || w == 0 || h == 0) return;
    set_window(x, y, x + w - 1, y + h - 1);

    uint8_t line[LCD_WIDTH * 2];
    uint8_t bg_h = (uint8_t)(s_bg >> 8), bg_l = (uint8_t)(s_bg & 0xFF);
    uint8_t fg_h = (uint8_t)(s_fg >> 8), fg_l = (uint8_t)(s_fg & 0xFF);
    uint16_t row_bytes = LCD_ROW_BYTES_1BPP;

    for (uint16_t ry = 0; ry < h; ry++) {
        const uint8_t *row = &mono_fb[(y + ry) * row_bytes];
        for (uint16_t rx = 0; rx < w; rx++) {
            uint16_t px = x + rx;
            int bit = (row[px >> 3] >> (7 - (px & 7))) & 1;
            if (bit) {
                line[rx * 2]     = bg_h;
                line[rx * 2 + 1] = bg_l;
            } else {
                line[rx * 2]     = fg_h;
                line[rx * 2 + 1] = fg_l;
            }
        }
        send_data_buf(line, w * 2);
    }
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data) {
    if (!rgb565_data || w == 0 || h == 0) return;
    set_window(x, y, x + w - 1, y + h - 1);
    for (uint16_t r = 0; r < h; r++) {
        send_data_buf(&rgb565_data[(size_t)r * w * 2], w * 2);
    }
}

void hal_display_present(void) {
    // RP2040 pushes via SPI DMA directly during flush
}

void hal_display_sleep(void) {
    send_command(0x10); // Sleep in
}

#endif
