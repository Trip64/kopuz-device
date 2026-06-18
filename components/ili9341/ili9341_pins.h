#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define ILI_SPI_HOST     SPI2_HOST
#define ILI_SPI_CLOCK_HZ (40 * 1000 * 1000)

#define ILI_PIN_MOSI   GPIO_NUM_39
#define ILI_PIN_SCLK   GPIO_NUM_41
#define ILI_PIN_CS     GPIO_NUM_42
#define ILI_PIN_DC     GPIO_NUM_2
#define ILI_PIN_RST    GPIO_NUM_1
#define ILI_PIN_BL     GPIO_NUM_8

/// RGB565 colours. Foreground = black ink, background = white, to match the
/// monochrome UI's look. Stored big-endian for the panel.
#define ILI_COLOR_BG 0xFFFF
#define ILI_COLOR_FG 0x0000
