#if defined(ESP_PLATFORM)

#include "hal/hal_display.h"
#include "config.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define PIN_LCD_CS      6
#define PIN_LCD_DC      7
#define PIN_LCD_WR      8
#define PIN_LCD_RD      9
#define PIN_LCD_RST     5
#define PIN_LCD_BL      38
#define PIN_LCD_PWR     15

#define PIN_LCD_D0      39
#define PIN_LCD_D1      40
#define PIN_LCD_D2      41
#define PIN_LCD_D3      42
#define PIN_LCD_D4      45
#define PIN_LCD_D5      46
#define PIN_LCD_D6      47
#define PIN_LCD_D7      48

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;

int hal_display_init(void) {
    // Power on LCD rail
    gpio_config_t pwr_cfg = {
        .pin_bit_mask = 1ULL << PIN_LCD_PWR,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pwr_cfg);
    gpio_set_level(PIN_LCD_PWR, 1);

    // RD high (read disabled)
    gpio_config_t rd_cfg = {
        .pin_bit_mask = 1ULL << PIN_LCD_RD,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rd_cfg);
    gpio_set_level(PIN_LCD_RD, 1);

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = PIN_LCD_DC,
        .wr_gpio_num = PIN_LCD_WR,
        .data_gpio_nums = {
            PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
            PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
    };
    esp_lcd_new_i80_bus(&bus_config, &i80_bus);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 20 * 1000 * 1000,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle);

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel_handle);

    esp_lcd_panel_reset(s_panel_handle);
    esp_lcd_panel_init(s_panel_handle);
    esp_lcd_panel_invert_color(s_panel_handle, true);
    esp_lcd_panel_swap_xy(s_panel_handle, true);
    esp_lcd_panel_mirror(s_panel_handle, false, true);
    esp_lcd_panel_set_gap(s_panel_handle, 0, 35);
    esp_lcd_panel_disp_on_off(s_panel_handle, true);

    // Backlight on
    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_cfg);
    gpio_set_level(PIN_LCD_BL, 1);

    hal_display_clear();
    return 0;
}

void hal_display_set_brightness(uint8_t pct) {
    (void)pct;
}

void hal_display_set_theme(uint16_t fg, uint16_t bg) {
    s_fg = fg;
    s_bg = bg;
}

void hal_display_clear(void) {
    if (!s_panel_handle) return;
    static uint16_t line[LCD_WIDTH];
    for (int i = 0; i < LCD_WIDTH; i++) line[i] = s_bg;

    for (int y = 0; y < LCD_HEIGHT; y++) {
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, LCD_WIDTH, y + 1, line);
    }
}

void hal_display_flush(const uint8_t *mono_fb) {
    if (!s_panel_handle || !mono_fb) return;

    static uint16_t line[LCD_WIDTH];
    uint16_t row_bytes = LCD_ROW_BYTES_1BPP;

    for (int y = 0; y < LCD_HEIGHT; y++) {
        const uint8_t *row = &mono_fb[y * row_bytes];
        for (int x = 0; x < LCD_WIDTH; x++) {
            int bit = (row[x >> 3] >> (7 - (x & 7))) & 1;
            line[x] = bit ? s_bg : s_fg;
        }
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, LCD_WIDTH, y + 1, line);
    }
}

void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!s_panel_handle || !mono_fb || w == 0 || h == 0) return;

    static uint16_t line[LCD_WIDTH];
    uint16_t row_bytes = LCD_ROW_BYTES_1BPP;

    for (uint16_t ry = 0; ry < h; ry++) {
        uint16_t py = y + ry;
        const uint8_t *row = &mono_fb[py * row_bytes];
        for (uint16_t rx = 0; rx < w; rx++) {
            uint16_t px = x + rx;
            int bit = (row[px >> 3] >> (7 - (px & 7))) & 1;
            line[rx] = bit ? s_bg : s_fg;
        }
        esp_lcd_panel_draw_bitmap(s_panel_handle, x, py, x + w, py + 1, line);
    }
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data) {
    if (!s_panel_handle || !rgb565_data || w == 0 || h == 0) return;
    esp_lcd_panel_draw_bitmap(s_panel_handle, x, y, x + w, y + h, rgb565_data);
}

void hal_display_present(void) {
    // ESP-IDF pushes via Intel 8080 DMA directly during flush
}

void hal_display_sleep(void) {
    if (s_panel_handle) esp_lcd_panel_disp_on_off(s_panel_handle, false);
}

#endif
