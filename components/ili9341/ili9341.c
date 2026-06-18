#include "ili9341.h"
#include "ili9341_pins.h"

#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// Backlight is PWM'd for brightness control. Uses LEDC timer 1 / channel 1 —
// the audio sink owns timer 0 / channel 0 (GPIO15), so no clash.
#define BL_LEDC_TIMER   LEDC_TIMER_1
#define BL_LEDC_CHANNEL LEDC_CHANNEL_1
#define BL_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define BL_LEDC_RES     LEDC_TIMER_8_BIT
#define BL_LEDC_FREQ    5000

static const char *TAG = "ili9341";
static spi_device_handle_t s_spi;
static bool s_inited;

// 1bpp UI colours (ink / background), RGB565. Default = dark theme. Changed at
// runtime by ili9341_set_theme.
static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;

void ili9341_set_theme(uint16_t fg, uint16_t bg) {
    s_fg = fg;
    s_bg = bg;
}

static void spi_write(const uint8_t *data, size_t len) {
    if (len == 0) return;
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void send_command(uint8_t cmd) {
    gpio_set_level(ILI_PIN_DC, 0);
    spi_write(&cmd, 1);
}

static void send_data_buf(const uint8_t *buf, size_t len) {
    gpio_set_level(ILI_PIN_DC, 1);
    spi_write(buf, len);
}

static void send_data(uint8_t d) {
    send_data_buf(&d, 1);
}

static void reset(void) {
    gpio_set_level(ILI_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(ILI_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(ILI_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
}

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[16];
} ili_init_cmd_t;

/// Standard ILI9341 power/gamma init (Adafruit sequence). MADCTL 0x28 puts the
/// panel in 320x240 landscape (MV set, BGR).
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
    {0x36, 1, {0x28}},
    {0x3A, 1, {0x55}},
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

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t cax[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    uint8_t pax[4] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    send_command(0x2A);
    send_data_buf(cax, 4);
    send_command(0x2B);
    send_data_buf(pax, 4);
    send_command(0x2C);
}

static void set_addr_window(void) {
    set_window(0, 0, ILI_WIDTH - 1, ILI_HEIGHT - 1);
}

void ili9341_display_region(const uint8_t *mono, uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h) {
    if (!s_inited || w == 0 || h == 0) return;
    if (x + w > ILI_WIDTH || y + h > ILI_HEIGHT) return;
    static uint8_t line[ILI_WIDTH * 2];
    set_window(x, y, x + w - 1, y + h - 1);
    for (uint16_t ry = 0; ry < h; ry++) {
        const uint8_t *row = &mono[(size_t)(y + ry) * ILI_ROW_BYTES];
        for (uint16_t rx = 0; rx < w; rx++) {
            uint16_t px = x + rx;
            int bit = (row[px >> 3] >> (7 - (px & 7))) & 1;
            uint16_t c = bit ? s_bg : s_fg;
            line[rx * 2] = c >> 8;
            line[rx * 2 + 1] = c & 0xFF;
        }
        send_data_buf(line, (size_t)w * 2);
    }
}

void ili9341_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                         const uint8_t *data) {
    if (!s_inited || w == 0 || h == 0) return;
    if (x + w > ILI_WIDTH || y + h > ILI_HEIGHT) return;
    set_window(x, y, x + w - 1, y + h - 1);
    // One row per transfer keeps each under max_transfer_sz.
    for (uint16_t r = 0; r < h; r++) {
        send_data_buf(&data[(size_t)r * w * 2], (size_t)w * 2);
    }
}

int ili9341_init(void) {
    if (s_inited) {
        reset();
    } else {
        gpio_config_t out = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << ILI_PIN_DC) | (1ULL << ILI_PIN_RST),
        };
        ESP_ERROR_CHECK(gpio_config(&out));

        ledc_timer_config_t bt = {
            .speed_mode = BL_LEDC_MODE,
            .timer_num = BL_LEDC_TIMER,
            .duty_resolution = BL_LEDC_RES,
            .freq_hz = BL_LEDC_FREQ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ledc_timer_config(&bt);
        ledc_channel_config_t bc = {
            .speed_mode = BL_LEDC_MODE,
            .channel = BL_LEDC_CHANNEL,
            .timer_sel = BL_LEDC_TIMER,
            .gpio_num = ILI_PIN_BL,
            .duty = 255,
            .hpoint = 0,
        };
        ledc_channel_config(&bc);

        spi_bus_config_t bus = {
            .mosi_io_num = ILI_PIN_MOSI,
            .sclk_io_num = ILI_PIN_SCLK,
            .miso_io_num = -1,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = ILI_WIDTH * 2 + 8,
        };
        ESP_ERROR_CHECK(spi_bus_initialize(ILI_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

        spi_device_interface_config_t dev = {
            .clock_speed_hz = ILI_SPI_CLOCK_HZ,
            .mode = 0,
            .spics_io_num = ILI_PIN_CS,
            .queue_size = 1,
        };
        ESP_ERROR_CHECK(spi_bus_add_device(ILI_SPI_HOST, &dev, &s_spi));
        s_inited = true;
        reset();
    }

    for (size_t i = 0; i < INIT_SEQ_LEN; i++) {
        send_command(INIT_SEQ[i].cmd);
        if (INIT_SEQ[i].len) {
            send_data_buf(INIT_SEQ[i].data, INIT_SEQ[i].len);
        }
    }
    send_command(0x11); // sleep out
    vTaskDelay(pdMS_TO_TICKS(120));
    send_command(0x29); // display on
    vTaskDelay(pdMS_TO_TICKS(20));

    ili9341_set_brightness(100); // backlight full on

    ESP_LOGI(TAG, "panel up (%dx%d)", ILI_WIDTH, ILI_HEIGHT);
    return 0;
}

void ili9341_set_brightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    uint32_t duty = ((uint32_t)pct * 255) / 100;
    ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}

static void fill_color(uint16_t color) {
    static uint8_t line[ILI_WIDTH * 2];
    uint8_t hi = color >> 8, lo = color & 0xFF;
    for (int x = 0; x < ILI_WIDTH; x++) {
        line[x * 2] = hi;
        line[x * 2 + 1] = lo;
    }
    set_addr_window();
    for (int y = 0; y < ILI_HEIGHT; y++) {
        send_data_buf(line, sizeof(line));
    }
}

void ili9341_clear(void) {
    fill_color(s_bg);
}

void ili9341_display_frame(const uint8_t *mono) {
    static uint8_t line[ILI_WIDTH * 2];
    const uint8_t fg_hi = s_fg >> 8, fg_lo = s_fg & 0xFF;
    const uint8_t bg_hi = s_bg >> 8, bg_lo = s_bg & 0xFF;

    set_addr_window();
    for (int y = 0; y < ILI_HEIGHT; y++) {
        const uint8_t *row = &mono[y * ILI_ROW_BYTES];
        for (int x = 0; x < ILI_WIDTH; x++) {
            int bit = (row[x >> 3] >> (7 - (x & 7))) & 1; // 1 = background
            if (bit) {
                line[x * 2] = bg_hi;
                line[x * 2 + 1] = bg_lo;
            } else {
                line[x * 2] = fg_hi;
                line[x * 2 + 1] = fg_lo;
            }
        }
        send_data_buf(line, sizeof(line));
    }
}
