#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define EPD_SPI_HOST   SPI2_HOST
#define EPD_SPI_CLOCK_HZ (4 * 1000 * 1000)

#define EPD_PIN_MOSI   GPIO_NUM_39
#define EPD_PIN_SCLK   GPIO_NUM_41

#define EPD_PIN_CS     GPIO_NUM_42
#define EPD_PIN_DC     GPIO_NUM_2
#define EPD_PIN_RST    GPIO_NUM_1
#define EPD_PIN_BUSY   GPIO_NUM_8
