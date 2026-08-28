/**
 ******************************************************************************
 * @file           : ssd1963.h
 * @brief          : SSD1963 TFT Display Driver for mikromedia 4
 ******************************************************************************
 */
#ifndef __SSD1963_H
#define __SSD1963_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ========================= Color Definitions ========================= */
/* RGB565 format: RRRRRGGGGGGBBBBB */
#define RGB565(r, g, b)     ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F
#define COLOR_ORANGE        0xFD20
#define COLOR_PURPLE        0x8010
#define COLOR_GRAY          0x8410
#define COLOR_DARK_GRAY     0x4208
#define COLOR_LIGHT_GRAY    0xC618

/* Premium color palette */
#define COLOR_NAVY          0x000F
#define COLOR_TEAL          0x0410
#define COLOR_CRIMSON       0xD8A7
#define COLOR_CORAL         0xFBEA
#define COLOR_GOLD          0xFEA0
#define COLOR_INDIGO        0x4810
#define COLOR_LIME          0x87E0
#define COLOR_SLATE         0x7BEF

/* ========================= SSD1963 Commands ========================= */
#define SSD1963_NOP                 0x00
#define SSD1963_SOFT_RESET          0x01
#define SSD1963_GET_POWER_MODE      0x0A
#define SSD1963_GET_ADDRESS_MODE    0x0B
#define SSD1963_ENTER_SLEEP_MODE    0x10
#define SSD1963_EXIT_SLEEP_MODE     0x11
#define SSD1963_ENTER_PARTIAL_MODE  0x12
#define SSD1963_ENTER_NORMAL_MODE   0x13
#define SSD1963_EXIT_INVERT_MODE    0x20
#define SSD1963_ENTER_INVERT_MODE   0x21
#define SSD1963_SET_DISPLAY_OFF     0x28
#define SSD1963_SET_DISPLAY_ON      0x29
#define SSD1963_SET_COLUMN_ADDRESS  0x2A
#define SSD1963_SET_PAGE_ADDRESS    0x2B
#define SSD1963_WRITE_MEMORY_START  0x2C
#define SSD1963_READ_MEMORY_START   0x2E
#define SSD1963_SET_TEAR_OFF        0x34
#define SSD1963_SET_TEAR_ON         0x35
#define SSD1963_SET_ADDRESS_MODE    0x36
#define SSD1963_SET_PIXEL_FORMAT    0x3A
#define SSD1963_SET_SCROLL_START    0x37
#define SSD1963_WRITE_MEMORY_CONT   0x3C
#define SSD1963_SET_PLL             0xE0
#define SSD1963_SET_PLL_MN          0xE2
#define SSD1963_SET_LSHIFT_FREQ     0xE6
#define SSD1963_SET_LCD_MODE        0xB0
#define SSD1963_SET_HORI_PERIOD     0xB4
#define SSD1963_SET_VERT_PERIOD     0xB6
#define SSD1963_SET_GPIO_CONF       0xB8
#define SSD1963_SET_GPIO_VALUE      0xBA
#define SSD1963_SET_POST_PROC       0xBC
#define SSD1963_SET_PWM_CONF        0xBE
#define SSD1963_SET_DBC_CONF        0xD0

/* ========================= Function Prototypes ========================= */

/**
 * @brief  Initialize SSD1963 display controller
 * @retval None
 */
void SSD1963_Init(void);

/**
 * @brief  Write a command to SSD1963
 * @param  cmd: Command byte
 * @retval None
 */
void SSD1963_WriteCommand(uint8_t cmd);

/**
 * @brief  Write data to SSD1963
 * @param  data: Data byte
 * @retval None
 */
void SSD1963_WriteData(uint8_t data);

/**
 * @brief  Write 16-bit data to SSD1963
 * @param  data: 16-bit data value
 * @retval None
 */
void SSD1963_WriteData16(uint16_t data);

/**
 * @brief  Set drawing window
 * @param  x0: Start X coordinate
 * @param  y0: Start Y coordinate
 * @param  x1: End X coordinate
 * @param  y1: End Y coordinate
 * @retval None
 */
void SSD1963_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief  Draw a single pixel
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  color: RGB565 color value
 * @retval None
 */
void SSD1963_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief  Fill entire screen with a color
 * @param  color: RGB565 color value
 * @retval None
 */
void SSD1963_FillScreen(uint16_t color);

/**
 * @brief  Fill a rectangular area with a color
 * @param  x: Start X coordinate
 * @param  y: Start Y coordinate
 * @param  w: Width
 * @param  h: Height
 * @param  color: RGB565 color value
 * @retval None
 */
void SSD1963_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  Set backlight brightness
 * @param  level: Brightness level (0-100)
 * @retval None
 */
void SSD1963_SetBacklight(uint8_t level);

/**
 * @brief  Hardware reset the display
 * @retval None
 */
void SSD1963_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __SSD1963_H */
