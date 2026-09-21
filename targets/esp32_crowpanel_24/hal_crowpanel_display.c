#if defined(ESP_PLATFORM)

#include "hal/hal_display.h"
#include "config.h"
#include "crowpanel_pins.h"

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LCD_CLOCK_HZ (16 * 1000 * 1000)

static const char *TAG = "crow_lcd";
static spi_device_handle_t s_lcd = NULL;
static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;

typedef struct {
    uint8_t command;
    uint8_t length;
    uint8_t data[16];
} ili9341_init_command_t;

static const ili9341_init_command_t INITIALIZATION[] = {
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
    {0x36, 1, {0x28}}, /* Landscape, BGR. */
    {0x3A, 1, {0x55}}, /* RGB565. */
    {0xB1, 2, {0x00, 0x18}},
    {0xB6, 3, {0x08, 0x82, 0x27}},
    {0xF2, 1, {0x00}},
    {0x26, 1, {0x01}},
    {0xE0, 15, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}},
    {0xE1, 15, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}},
};

static esp_err_t transmit(bool data_mode, const void *bytes, size_t length) {
    if (!s_lcd || !bytes || length == 0) return ESP_ERR_INVALID_ARG;
    gpio_set_level(CROW_LCD_DC, data_mode ? 1 : 0);

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.length = length * 8;
    transaction.tx_buffer = bytes;
    return spi_device_polling_transmit(s_lcd, &transaction);
}

static esp_err_t send_command(uint8_t command) {
    return transmit(false, &command, sizeof(command));
}

static esp_err_t send_data(const void *data, size_t length) {
    return transmit(true, data, length);
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t columns[4] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    uint8_t rows[4] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0,
        (uint8_t)(y1 >> 8), (uint8_t)y1,
    };
    send_command(0x2A);
    send_data(columns, sizeof(columns));
    send_command(0x2B);
    send_data(rows, sizeof(rows));
    send_command(0x2C);
}

static bool clip_rectangle(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h) {
    if (!x || !y || !w || !h || *w == 0 || *h == 0) return false;
    if (*x >= LCD_WIDTH || *y >= LCD_HEIGHT) return false;
    if ((uint32_t)*x + *w > LCD_WIDTH) *w = (uint16_t)(LCD_WIDTH - *x);
    if ((uint32_t)*y + *h > LCD_HEIGHT) *h = (uint16_t)(LCD_HEIGHT - *y);
    return *w > 0 && *h > 0;
}

int hal_display_init(void) {
    /* Keep every device deselected before either SPI bus starts. */
    gpio_config_t cs_config = {
        .pin_bit_mask = (1ULL << CROW_LCD_CS) |
                        (1ULL << CROW_TOUCH_CS) |
                        (1ULL << CROW_SD_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_config));
    gpio_set_level(CROW_LCD_CS, 1);
    gpio_set_level(CROW_TOUCH_CS, 1);
    gpio_set_level(CROW_SD_CS, 1);

    gpio_config_t dc_config = {
        .pin_bit_mask = 1ULL << CROW_LCD_DC,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&dc_config));
    gpio_set_level(CROW_LCD_DC, 1);

    spi_bus_config_t bus_config = {
        .mosi_io_num = CROW_LCD_MOSI,
        .miso_io_num = CROW_LCD_MISO,
        .sclk_io_num = CROW_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 2,
    };
    esp_err_t error = spi_bus_initialize(CROW_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "LCD SPI bus initialization failed: %s", esp_err_to_name(error));
        return -1;
    }

    spi_device_interface_config_t device_config = {
        .clock_speed_hz = LCD_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = CROW_LCD_CS,
        .queue_size = 1,
    };
    error = spi_bus_add_device(CROW_LCD_HOST, &device_config, &s_lcd);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "ILI9341 device initialization failed: %s", esp_err_to_name(error));
        spi_bus_free(CROW_LCD_HOST);
        return -1;
    }

    /* The board ties LCD reset to the ESP32 reset line. */
    vTaskDelay(pdMS_TO_TICKS(120));
    for (size_t i = 0; i < sizeof(INITIALIZATION) / sizeof(INITIALIZATION[0]); ++i) {
        if (send_command(INITIALIZATION[i].command) != ESP_OK ||
            (INITIALIZATION[i].length > 0 &&
             send_data(INITIALIZATION[i].data, INITIALIZATION[i].length) != ESP_OK)) {
            ESP_LOGE(TAG, "ILI9341 initialization failed at command 0x%02x",
                     INITIALIZATION[i].command);
            return -1;
        }
    }
    send_command(0x11); /* Sleep out. */
    vTaskDelay(pdMS_TO_TICKS(120));
    send_command(0x29); /* Display on. */

    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));
    ledc_channel_config_t channel_config = {
        .gpio_num = CROW_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 255,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));

    ESP_LOGI(TAG, "ILI9341 ready on HSPI at %d MHz", LCD_CLOCK_HZ / 1000000);
    hal_display_clear();
    return 0;
}

void hal_display_set_brightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t duty = ((uint32_t)percent * 255U) / 100U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void hal_display_set_theme(uint16_t foreground, uint16_t background) {
    s_fg = foreground;
    s_bg = background;
}

void hal_display_clear(void) {
    if (!s_lcd) return;
    uint8_t line[LCD_WIDTH * 2];
    for (size_t x = 0; x < LCD_WIDTH; ++x) {
        line[x * 2] = (uint8_t)(s_bg >> 8);
        line[x * 2 + 1] = (uint8_t)s_bg;
    }
    set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    for (size_t y = 0; y < LCD_HEIGHT; ++y) send_data(line, sizeof(line));
}

void hal_display_flush(const uint8_t *mono_framebuffer) {
    if (!s_lcd || !mono_framebuffer) return;

    uint8_t line[LCD_WIDTH * 2];
    set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    for (size_t y = 0; y < LCD_HEIGHT; ++y) {
        const uint8_t *row = &mono_framebuffer[y * LCD_ROW_BYTES_1BPP];
        for (size_t x = 0; x < LCD_WIDTH; ++x) {
            bool background = ((row[x >> 3] >> (7 - (x & 7))) & 1U) != 0;
            uint16_t color = background ? s_bg : s_fg;
            line[x * 2] = (uint8_t)(color >> 8);
            line[x * 2 + 1] = (uint8_t)color;
        }
        send_data(line, sizeof(line));
    }
}

void hal_display_flush_region(const uint8_t *mono_framebuffer, uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height) {
    if (!s_lcd || !mono_framebuffer || !clip_rectangle(&x, &y, &width, &height)) return;

    uint8_t line[LCD_WIDTH * 2];
    set_window(x, y, x + width - 1, y + height - 1);
    for (uint16_t row_index = 0; row_index < height; ++row_index) {
        const uint8_t *row = &mono_framebuffer[(y + row_index) * LCD_ROW_BYTES_1BPP];
        for (uint16_t column = 0; column < width; ++column) {
            uint16_t pixel_x = x + column;
            bool background = ((row[pixel_x >> 3] >> (7 - (pixel_x & 7))) & 1U) != 0;
            uint16_t color = background ? s_bg : s_fg;
            line[column * 2] = (uint8_t)(color >> 8);
            line[column * 2 + 1] = (uint8_t)color;
        }
        send_data(line, (size_t)width * 2);
    }
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                             const uint8_t *rgb565_data) {
    uint16_t requested_width = width;
    if (!s_lcd || !rgb565_data || !clip_rectangle(&x, &y, &width, &height)) return;
    set_window(x, y, x + width - 1, y + height - 1);
    for (uint16_t row = 0; row < height; ++row) {
        send_data(&rgb565_data[(size_t)row * requested_width * 2], (size_t)width * 2);
    }
}

void hal_display_present(void) {
}

void hal_display_sleep(void) {
    if (!s_lcd) return;
    hal_display_set_brightness(0);
    send_command(0x10);
}

#endif
