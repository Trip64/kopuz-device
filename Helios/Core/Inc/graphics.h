/**
 ******************************************************************************
 * @file           : graphics.h
 * @brief          : Graphics primitives library for TFT display
 ******************************************************************************
 */
#ifndef __GRAPHICS_H
#define __GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ssd1963.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================= Graphics Functions ========================= */

/**
 * @brief  Draw a horizontal line
 * @param  x: Start X coordinate
 * @param  y: Y coordinate
 * @param  w: Width (length)
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);

/**
 * @brief  Draw a vertical line
 * @param  x: X coordinate
 * @param  y: Start Y coordinate
 * @param  h: Height (length)
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);

/**
 * @brief  Draw a line between two points (Bresenham)
 * @param  x0,y0: Start point
 * @param  x1,y1: End point
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

/**
 * @brief  Draw a rectangle outline
 * @param  x,y: Top-left corner
 * @param  w,h: Width and height
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  Draw a filled rectangle
 * @param  x,y: Top-left corner
 * @param  w,h: Width and height
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  Draw a rounded rectangle outline
 * @param  x,y: Top-left corner
 * @param  w,h: Width and height
 * @param  r: Corner radius
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color);

/**
 * @brief  Draw a filled rounded rectangle
 * @param  x,y: Top-left corner
 * @param  w,h: Width and height
 * @param  r: Corner radius
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color);

/**
 * @brief  Draw a circle outline
 * @param  x0,y0: Center point
 * @param  r: Radius
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);

/**
 * @brief  Draw a filled circle
 * @param  x0,y0: Center point
 * @param  r: Radius
 * @param  color: RGB565 color
 * @retval None
 */
void GFX_FillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);

/**
 * @brief  Draw a single character
 * @param  x,y: Top-left position
 * @param  c: Character to draw
 * @param  color: Text color
 * @param  bg: Background color
 * @param  size: Font size multiplier (1 = 8x16, 2 = 16x32, etc.)
 * @retval None
 */
void GFX_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);

/**
 * @brief  Draw a string
 * @param  x,y: Top-left position
 * @param  str: Null-terminated string
 * @param  color: Text color
 * @param  bg: Background color
 * @param  size: Font size multiplier
 * @retval None
 */
void GFX_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

/**
 * @brief  Draw a string with transparent background
 * @param  x,y: Top-left position
 * @param  str: Null-terminated string
 * @param  color: Text color
 * @param  size: Font size multiplier
 * @retval None
 */
void GFX_DrawStringTransparent(uint16_t x, uint16_t y, const char *str, uint16_t color, uint8_t size);

/**
 * @brief  Draw a gradient-filled rectangle (vertical gradient)
 * @param  x,y: Top-left corner
 * @param  w,h: Width and height
 * @param  color1: Top color
 * @param  color2: Bottom color
 * @retval None
 */
void GFX_FillGradientV(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color1, uint16_t color2);

/**
 * @brief  Draw a gradient-filled rectangle (horizontal gradient)
 * @param  x,y: Top-left corner
 * @param  w,h: Width and height
 * @param  color1: Left color
 * @param  color2: Right color
 * @retval None
 */
void GFX_FillGradientH(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color1, uint16_t color2);

/**
 * @brief  Blend two colors
 * @param  color1: First color
 * @param  color2: Second color
 * @param  alpha: Blend factor (0-255, 0 = color1, 255 = color2)
 * @retval Blended color
 */
uint16_t GFX_BlendColors(uint16_t color1, uint16_t color2, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif /* __GRAPHICS_H */
