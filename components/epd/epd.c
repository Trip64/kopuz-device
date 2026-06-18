#include "epd.h"
#include "epd_pins.h"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "epd";
static spi_device_handle_t s_spi;
static bool s_inited;

static void busy_wait(void) {
    const int timeout_ms = 5000;
    int start_level = gpio_get_level(EPD_PIN_BUSY);
    int waited = 0;
    while (gpio_get_level(EPD_PIN_BUSY) == 1) {
        vTaskDelay(pdMS_TO_TICKS(5));
        waited += 5;
        if (waited >= timeout_ms) {
            ESP_LOGW(TAG, "BUSY stuck high %dms — check BUSY/RST/CS wiring",
                     timeout_ms);
            return;
        }
    }
    ESP_LOGI(TAG, "busy_wait: start=%d held=%dms", start_level, waited);
}

static void reset(void) {
    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(EPD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
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
    gpio_set_level(EPD_PIN_DC, 0);
    spi_write(&cmd, 1);
}

static void send_data(uint8_t data) {
    gpio_set_level(EPD_PIN_DC, 1);
    spi_write(&data, 1);
}

static void send_data_buf(const uint8_t *buf, size_t len) {
    gpio_set_level(EPD_PIN_DC, 1);
    spi_write(buf, len);
}

static void set_window_full(void) {
    send_command(0x44);
    send_data(0x00);
    send_data((EPD_WIDTH - 1) >> 3);
    send_command(0x45);
    send_data(0x00);
    send_data(0x00);
    send_data((EPD_HEIGHT - 1) & 0xFF);
    send_data(((EPD_HEIGHT - 1) >> 8) & 0xFF);
    send_command(0x4E);
    send_data(0x00);
    send_command(0x4F);
    send_data(0x00);
    send_data(0x00);
}

int epd_init(void) {
    if (s_inited) {
        reset();
    } else {
        gpio_config_t out = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << EPD_PIN_DC) | (1ULL << EPD_PIN_RST),
        };
        ESP_ERROR_CHECK(gpio_config(&out));
        gpio_config_t in = {
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << EPD_PIN_BUSY),
        };
        ESP_ERROR_CHECK(gpio_config(&in));

        spi_bus_config_t bus = {
            .mosi_io_num = EPD_PIN_MOSI,
            .sclk_io_num = EPD_PIN_SCLK,
            .miso_io_num = -1,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = EPD_FRAME_BYTES + 8,
        };
        ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

        spi_device_interface_config_t dev = {
            .clock_speed_hz = EPD_SPI_CLOCK_HZ,
            .mode = 0,
            .spics_io_num = EPD_PIN_CS,
            .queue_size = 1,
        };
        ESP_ERROR_CHECK(spi_bus_add_device(EPD_SPI_HOST, &dev, &s_spi));
        s_inited = true;
        reset();
    }

    busy_wait();
    send_command(0x12);
    busy_wait();

    send_command(0x01);
    send_data((EPD_HEIGHT - 1) & 0xFF);
    send_data(((EPD_HEIGHT - 1) >> 8) & 0xFF);
    send_data(0x00);

    send_command(0x11); send_data(0x03);
    set_window_full();

    send_command(0x3C); send_data(0x05);
    send_command(0x21);
    send_data(0x00);
    send_data(0x80);
    send_command(0x18); send_data(0x80);
    busy_wait();

    ESP_LOGI(TAG, "panel up (%dx%d)", EPD_WIDTH, EPD_HEIGHT);
    return 0;
}

static void turn_on_display(void) {
    send_command(0x22);
    send_data(0xF7);
    send_command(0x20);
    busy_wait();
}

static void turn_on_display_partial(void) {
    send_command(0x22);
    send_data(0xFF);
    send_command(0x20);
    busy_wait();
}

static const uint8_t LUT_PARTIAL[159] = {
    0x0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x80, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x40, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x14, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0,
    0x22, 0x17, 0x41, 0xB0, 0x32, 0x36,
};

static void load_partial_lut(void) {
    send_command(0x32);
    send_data_buf(LUT_PARTIAL, 153);
    busy_wait();
    send_command(0x3F); send_data(LUT_PARTIAL[153]);
    send_command(0x03); send_data(LUT_PARTIAL[154]);
    send_command(0x04);
    send_data(LUT_PARTIAL[155]);
    send_data(LUT_PARTIAL[156]);
    send_data(LUT_PARTIAL[157]);
    send_command(0x2C); send_data(LUT_PARTIAL[158]);
}

void epd_display_frame(const uint8_t *buf) {
    set_window_full();
    send_command(0x24);
    send_data_buf(buf, EPD_FRAME_BYTES);
    turn_on_display();
    set_window_full();
    send_command(0x26);
    send_data_buf(buf, EPD_FRAME_BYTES);
}

void epd_display_frame_partial(const uint8_t *buf) {
    gpio_set_level(EPD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(2));

    load_partial_lut();
    send_command(0x3C); send_data(0x80);
    send_command(0x22); send_data(0xC0);
    send_command(0x20);
    busy_wait();

    set_window_full();
    send_command(0x24);
    send_data_buf(buf, EPD_FRAME_BYTES);
    turn_on_display_partial();
}

void epd_clear(void) {
    uint8_t row[EPD_ROW_BYTES];
    memset(row, 0xFF, sizeof(row));
    set_window_full();
    send_command(0x24);
    for (int y = 0; y < EPD_HEIGHT; y++) {
        send_data_buf(row, EPD_ROW_BYTES);
    }
    turn_on_display();
}

void epd_sleep(void) {
    send_command(0x10);
    send_data(0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
}
