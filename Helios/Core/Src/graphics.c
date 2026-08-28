/**
 ******************************************************************************
 * @file           : graphics.c
 * @brief          : Graphics primitives library implementation
 ******************************************************************************
 */
#include "graphics.h"
#include <stdlib.h>
#include <string.h>

/* 8x16 font data (ASCII 32-127) - basic font for text rendering */
static const uint8_t font8x16[][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* Space */
    {0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00}, /* ! */
    {0x00,0x63,0x63,0x63,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* " */
    {0x00,0x00,0x00,0x36,0x36,0x7F,0x36,0x36,0x36,0x7F,0x36,0x36,0x00,0x00,0x00,0x00}, /* # */
};

/* Simple 5x7 font for faster rendering */
static const uint8_t font5x7[] = {
    0x00,0x00,0x00,0x00,0x00, /* Space */
    0x00,0x00,0x5F,0x00,0x00, /* ! */
    0x00,0x07,0x00,0x07,0x00, /* " */
    0x14,0x7F,0x14,0x7F,0x14, /* # */
    0x24,0x2A,0x7F,0x2A,0x12, /* $ */
    0x23,0x13,0x08,0x64,0x62, /* % */
    0x36,0x49,0x55,0x22,0x50, /* & */
    0x00,0x00,0x07,0x00,0x00, /* ' */
    0x00,0x1C,0x22,0x41,0x00, /* ( */
    0x00,0x41,0x22,0x1C,0x00, /* ) */
    0x14,0x08,0x3E,0x08,0x14, /* * */
    0x08,0x08,0x3E,0x08,0x08, /* + */
    0x00,0x50,0x30,0x00,0x00, /* , */
    0x08,0x08,0x08,0x08,0x08, /* - */
    0x00,0x60,0x60,0x00,0x00, /* . */
    0x20,0x10,0x08,0x04,0x02, /* / */
    0x3E,0x51,0x49,0x45,0x3E, /* 0 */
    0x00,0x42,0x7F,0x40,0x00, /* 1 */
    0x42,0x61,0x51,0x49,0x46, /* 2 */
    0x21,0x41,0x45,0x4B,0x31, /* 3 */
    0x18,0x14,0x12,0x7F,0x10, /* 4 */
    0x27,0x45,0x45,0x45,0x39, /* 5 */
    0x3C,0x4A,0x49,0x49,0x30, /* 6 */
    0x01,0x71,0x09,0x05,0x03, /* 7 */
    0x36,0x49,0x49,0x49,0x36, /* 8 */
    0x06,0x49,0x49,0x29,0x1E, /* 9 */
    0x00,0x36,0x36,0x00,0x00, /* : */
    0x00,0x56,0x36,0x00,0x00, /* ; */
    0x08,0x14,0x22,0x41,0x00, /* < */
    0x14,0x14,0x14,0x14,0x14, /* = */
    0x00,0x41,0x22,0x14,0x08, /* > */
    0x02,0x01,0x51,0x09,0x06, /* ? */
    0x3E,0x41,0x5D,0x55,0x1E, /* @ */
    0x7E,0x11,0x11,0x11,0x7E, /* A */
    0x7F,0x49,0x49,0x49,0x36, /* B */
    0x3E,0x41,0x41,0x41,0x22, /* C */
    0x7F,0x41,0x41,0x22,0x1C, /* D */
    0x7F,0x49,0x49,0x49,0x41, /* E */
    0x7F,0x09,0x09,0x09,0x01, /* F */
    0x3E,0x41,0x49,0x49,0x7A, /* G */
    0x7F,0x08,0x08,0x08,0x7F, /* H */
    0x00,0x41,0x7F,0x41,0x00, /* I */
    0x20,0x40,0x41,0x3F,0x01, /* J */
    0x7F,0x08,0x14,0x22,0x41, /* K */
    0x7F,0x40,0x40,0x40,0x40, /* L */
    0x7F,0x02,0x0C,0x02,0x7F, /* M */
    0x7F,0x04,0x08,0x10,0x7F, /* N */
    0x3E,0x41,0x41,0x41,0x3E, /* O */
    0x7F,0x09,0x09,0x09,0x06, /* P */
    0x3E,0x41,0x51,0x21,0x5E, /* Q */
    0x7F,0x09,0x19,0x29,0x46, /* R */
    0x46,0x49,0x49,0x49,0x31, /* S */
    0x01,0x01,0x7F,0x01,0x01, /* T */
    0x3F,0x40,0x40,0x40,0x3F, /* U */
    0x1F,0x20,0x40,0x20,0x1F, /* V */
    0x3F,0x40,0x38,0x40,0x3F, /* W */
    0x63,0x14,0x08,0x14,0x63, /* X */
    0x07,0x08,0x70,0x08,0x07, /* Y */
    0x61,0x51,0x49,0x45,0x43, /* Z */
};

void GFX_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    SSD1963_FillRect(x, y, w, 1, color);
}

void GFX_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color) {
    SSD1963_FillRect(x, y, 1, h, color);
}

void GFX_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t steep = abs((int16_t)y1 - (int16_t)y0) > abs((int16_t)x1 - (int16_t)x0);
    int16_t dx, dy, err, ystep;
    
    if (steep) {
        uint16_t tmp = x0; x0 = y0; y0 = tmp;
        tmp = x1; x1 = y1; y1 = tmp;
    }
    if (x0 > x1) {
        uint16_t tmp = x0; x0 = x1; x1 = tmp;
        tmp = y0; y0 = y1; y1 = tmp;
    }
    
    dx = x1 - x0;
    dy = abs((int16_t)y1 - (int16_t)y0);
    err = dx / 2;
    ystep = (y0 < y1) ? 1 : -1;
    
    for (; x0 <= x1; x0++) {
        if (steep) SSD1963_DrawPixel(y0, x0, color);
        else SSD1963_DrawPixel(x0, y0, color);
        err -= dy;
        if (err < 0) { y0 += ystep; err += dx; }
    }
}

void GFX_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    GFX_DrawHLine(x, y, w, color);
    GFX_DrawHLine(x, y + h - 1, w, color);
    GFX_DrawVLine(x, y, h, color);
    GFX_DrawVLine(x + w - 1, y, h, color);
}

void GFX_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    SSD1963_FillRect(x, y, w, h, color);
}

static void drawCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t corner, uint16_t color) {
    int16_t f = 1 - r, fx = 1, fy = -2 * r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) { y--; fy += 2; f += fy; }
        x++; fx += 2; f += fx;
        if (corner & 0x4) { SSD1963_DrawPixel(x0+x,y0+y,color); SSD1963_DrawPixel(x0+y,y0+x,color); }
        if (corner & 0x2) { SSD1963_DrawPixel(x0+x,y0-y,color); SSD1963_DrawPixel(x0+y,y0-x,color); }
        if (corner & 0x8) { SSD1963_DrawPixel(x0-y,y0+x,color); SSD1963_DrawPixel(x0-x,y0+y,color); }
        if (corner & 0x1) { SSD1963_DrawPixel(x0-y,y0-x,color); SSD1963_DrawPixel(x0-x,y0-y,color); }
    }
}

void GFX_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int16_t f = 1 - r, fx = 1, fy = -2 * r, x = 0, y = r;
    SSD1963_DrawPixel(x0, y0+r, color); SSD1963_DrawPixel(x0, y0-r, color);
    SSD1963_DrawPixel(x0+r, y0, color); SSD1963_DrawPixel(x0-r, y0, color);
    while (x < y) {
        if (f >= 0) { y--; fy += 2; f += fy; }
        x++; fx += 2; f += fx;
        SSD1963_DrawPixel(x0+x,y0+y,color); SSD1963_DrawPixel(x0-x,y0+y,color);
        SSD1963_DrawPixel(x0+x,y0-y,color); SSD1963_DrawPixel(x0-x,y0-y,color);
        SSD1963_DrawPixel(x0+y,y0+x,color); SSD1963_DrawPixel(x0-y,y0+x,color);
        SSD1963_DrawPixel(x0+y,y0-x,color); SSD1963_DrawPixel(x0-y,y0-x,color);
    }
}

static void fillCircleHelper(uint16_t x0, uint16_t y0, uint16_t r, uint8_t corner, int16_t delta, uint16_t color) {
    int16_t f = 1-r, fx = 1, fy = -2*r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) { y--; fy += 2; f += fy; }
        x++; fx += 2; f += fx;
        if (corner & 0x1) { GFX_DrawVLine(x0+x, y0-y, 2*y+1+delta, color); GFX_DrawVLine(x0+y, y0-x, 2*x+1+delta, color); }
        if (corner & 0x2) { GFX_DrawVLine(x0-x, y0-y, 2*y+1+delta, color); GFX_DrawVLine(x0-y, y0-x, 2*x+1+delta, color); }
    }
}

void GFX_FillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    GFX_DrawVLine(x0, y0-r, 2*r+1, color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
}

void GFX_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
    GFX_DrawHLine(x+r, y, w-2*r, color);
    GFX_DrawHLine(x+r, y+h-1, w-2*r, color);
    GFX_DrawVLine(x, y+r, h-2*r, color);
    GFX_DrawVLine(x+w-1, y+r, h-2*r, color);
    drawCircleHelper(x+r, y+r, r, 1, color);
    drawCircleHelper(x+w-r-1, y+r, r, 2, color);
    drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
    drawCircleHelper(x+r, y+h-r-1, r, 8, color);
}

void GFX_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color) {
    GFX_FillRect(x+r, y, w-2*r, h, color);
    fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
    fillCircleHelper(x+r, y+r, r, 2, h-2*r-1, color);
}

void GFX_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 90) c = 32;
    uint8_t idx = c - 32;
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t line = font5x7[idx * 5 + i];
        for (uint8_t j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                if (size == 1) SSD1963_DrawPixel(x+i, y+j, color);
                else GFX_FillRect(x+i*size, y+j*size, size, size, color);
            } else if (bg != color) {
                if (size == 1) SSD1963_DrawPixel(x+i, y+j, bg);
                else GFX_FillRect(x+i*size, y+j*size, size, size, bg);
            }
        }
    }
}

void GFX_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    while (*str) {
        GFX_DrawChar(x, y, *str++, color, bg, size);
        x += 6 * size;
    }
}

void GFX_DrawStringTransparent(uint16_t x, uint16_t y, const char *str, uint16_t color, uint8_t size) {
    while (*str) {
        if (*str >= 32 && *str <= 90) {
            uint8_t idx = *str - 32;
            for (uint8_t i = 0; i < 5; i++) {
                uint8_t line = font5x7[idx * 5 + i];
                for (uint8_t j = 0; j < 7; j++) {
                    if (line & (1 << j)) {
                        if (size == 1) SSD1963_DrawPixel(x+i, y+j, color);
                        else GFX_FillRect(x+i*size, y+j*size, size, size, color);
                    }
                }
            }
        }
        str++; x += 6 * size;
    }
}

uint16_t GFX_BlendColors(uint16_t c1, uint16_t c2, uint8_t alpha) {
    uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
    uint8_t r = (r1 * (255-alpha) + r2 * alpha) / 255;
    uint8_t g = (g1 * (255-alpha) + g2 * alpha) / 255;
    uint8_t b = (b1 * (255-alpha) + b2 * alpha) / 255;
    return (r << 11) | (g << 5) | b;
}

void GFX_FillGradientV(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c1, uint16_t c2) {
    for (uint16_t i = 0; i < h; i++) {
        uint16_t color = GFX_BlendColors(c1, c2, (i * 255) / h);
        GFX_DrawHLine(x, y + i, w, color);
    }
}

void GFX_FillGradientH(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c1, uint16_t c2) {
    for (uint16_t i = 0; i < w; i++) {
        uint16_t color = GFX_BlendColors(c1, c2, (i * 255) / w);
        GFX_DrawVLine(x + i, y, h, color);
    }
}
