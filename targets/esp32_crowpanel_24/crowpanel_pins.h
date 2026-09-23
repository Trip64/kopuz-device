#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

/* Elecrow CrowPanel 2.4-inch ESP32 HMI, model DIS03024H. */
#define CROW_LCD_HOST       SPI2_HOST
#define CROW_LCD_MOSI       GPIO_NUM_13
#define CROW_LCD_MISO       GPIO_NUM_12
#define CROW_LCD_SCLK       GPIO_NUM_14
#define CROW_LCD_CS         GPIO_NUM_15
#define CROW_LCD_DC         GPIO_NUM_2
#define CROW_LCD_BL         GPIO_NUM_27
#define CROW_TOUCH_CS       GPIO_NUM_33

#define CROW_SD_HOST        SPI3_HOST
#define CROW_SD_MOSI        GPIO_NUM_23
#define CROW_SD_MISO        GPIO_NUM_19
#define CROW_SD_SCLK        GPIO_NUM_18
#define CROW_SD_CS          GPIO_NUM_5

#define CROW_BUTTON_1       GPIO_NUM_25
#define CROW_BUTTON_2       GPIO_NUM_32
#define CROW_SPEAKER        GPIO_NUM_26
