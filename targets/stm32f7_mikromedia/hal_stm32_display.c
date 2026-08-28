#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_display.h"
#include "config.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define TFT_DATA_LO_PORT    GPIOG   /* D0-D7 */
#define TFT_DATA_HI_PORT    GPIOE   /* D8-D15 */
#define TFT_DATA_LO_MASK    0x00FF  /* PG0-PG7 */
#define TFT_DATA_HI_MASK    0xFF00  /* PE8-PE15 */

#define TFT_BLED_PIN        GPIO_PIN_10
#define TFT_BLED_PORT       GPIOF
#define TFT_WR_PIN          GPIO_PIN_11
#define TFT_WR_PORT         GPIOF
#define TFT_RD_PIN          GPIO_PIN_12
#define TFT_RD_PORT         GPIOF
#define TFT_CS_PIN          GPIO_PIN_13
#define TFT_CS_PORT         GPIOF
#define TFT_RST_PIN         GPIO_PIN_14
#define TFT_RST_PORT        GPIOF
#define TFT_RS_PIN          GPIO_PIN_15
#define TFT_RS_PORT         GPIOF

/* Control pin macros using atomic BSRR register */
#define TFT_CS_LOW()        (TFT_CS_PORT->BSRR = (uint32_t)TFT_CS_PIN << 16)
#define TFT_CS_HIGH()       (TFT_CS_PORT->BSRR = TFT_CS_PIN)
#define TFT_RS_LOW()        (TFT_RS_PORT->BSRR = (uint32_t)TFT_RS_PIN << 16)
#define TFT_RS_HIGH()       (TFT_RS_PORT->BSRR = TFT_RS_PIN)
#define TFT_WR_LOW()        (TFT_WR_PORT->BSRR = (uint32_t)TFT_WR_PIN << 16)
#define TFT_WR_HIGH()       (TFT_WR_PORT->BSRR = TFT_WR_PIN)
#define TFT_RD_LOW()        (TFT_RD_PORT->BSRR = (uint32_t)TFT_RD_PIN << 16)
#define TFT_RD_HIGH()       (TFT_RD_PORT->BSRR = TFT_RD_PIN)
#define TFT_RST_LOW()       (TFT_RST_PORT->BSRR = (uint32_t)TFT_RST_PIN << 16)
#define TFT_RST_HIGH()      (TFT_RST_PORT->BSRR = TFT_RST_PIN)
#define TFT_BLED_ON()       (TFT_BLED_PORT->BSRR = TFT_BLED_PIN)
#define TFT_BLED_OFF()      (TFT_BLED_PORT->BSRR = (uint32_t)TFT_BLED_PIN << 16)

static inline void TFT_WRITE_BUS(uint16_t data) {
    uint32_t temp;
    temp = TFT_DATA_HI_PORT->ODR;
    temp &= ~TFT_DATA_HI_MASK;
    TFT_DATA_HI_PORT->ODR = temp | (data & 0xFF00);
    temp = TFT_DATA_LO_PORT->ODR;
    temp &= ~TFT_DATA_LO_MASK;
    TFT_DATA_LO_PORT->ODR = temp | (data & 0x00FF);
}

#define TFT_WR_STROBE() do { \
    __asm volatile("nop; nop"); \
    TFT_WR_LOW(); \
    __asm volatile("nop; nop; nop; nop; nop"); \
    TFT_WR_HIGH(); \
    __asm volatile("nop; nop"); \
} while(0)

static inline void TFT_WRITE_PIXEL(uint16_t color) {
    uint32_t temp;
    temp = TFT_DATA_HI_PORT->ODR & ~0xFF00;
    TFT_DATA_HI_PORT->ODR = temp | (color & 0xFF00);
    temp = TFT_DATA_LO_PORT->ODR & ~0x00FF;
    TFT_DATA_LO_PORT->ODR = temp | (color & 0x00FF);
    __asm volatile("nop; nop");
    TFT_WR_LOW();
    __asm volatile("nop; nop; nop; nop; nop");
    TFT_WR_HIGH();
    __asm volatile("nop; nop");
}

#define SSD1963_SOFT_RESET          0x01
#define SSD1963_SET_PLL             0xE0
#define SSD1963_SET_PLL_MN          0xE2
#define SSD1963_SET_LSHIFT_FREQ     0xE6
#define SSD1963_SET_LCD_MODE        0xB0
#define SSD1963_SET_HORI_PERIOD     0xB4
#define SSD1963_SET_VERT_PERIOD     0xB6
#define SSD1963_SET_ADDRESS_MODE    0x36
#define SSD1963_SET_PIXEL_FORMAT    0x3A
#define SSD1963_SET_COLUMN_ADDRESS  0x2A
#define SSD1963_SET_PAGE_ADDRESS    0x2B
#define SSD1963_WRITE_MEMORY_START  0x2C
#define SSD1963_SET_DISPLAY_ON      0x29
#define SSD1963_SET_DISPLAY_OFF     0x28
#define SSD1963_EXIT_SLEEP_MODE     0x11
#define SSD1963_SET_GPIO_CONF       0xBA
#define SSD1963_SET_PWM_CONF        0xBE

static uint16_t s_fg = 0xFFFF; // Default White ink
static uint16_t s_bg = 0x0841; // Default Dark Slate background
static uint8_t s_brightness = 100;

static void SSD1963_Reset(void) {
    TFT_RST_HIGH(); HAL_Delay(5);
    TFT_RST_LOW();  HAL_Delay(20);
    TFT_RST_HIGH(); HAL_Delay(150);
}

static void SSD1963_WriteCommand(uint8_t cmd) {
    TFT_CS_LOW(); TFT_RS_LOW();
    TFT_WRITE_BUS(cmd);
    TFT_WR_STROBE();
    TFT_CS_HIGH();
}

static void SSD1963_WriteData(uint8_t data) {
    TFT_CS_LOW(); TFT_RS_HIGH();
    TFT_WRITE_BUS(data);
    TFT_WR_STROBE();
    TFT_CS_HIGH();
}

static void SSD1963_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    SSD1963_WriteCommand(SSD1963_SET_COLUMN_ADDRESS);
    SSD1963_WriteData(x0 >> 8); SSD1963_WriteData(x0 & 0xFF);
    SSD1963_WriteData(x1 >> 8); SSD1963_WriteData(x1 & 0xFF);
    SSD1963_WriteCommand(SSD1963_SET_PAGE_ADDRESS);
    SSD1963_WriteData(y0 >> 8); SSD1963_WriteData(y0 & 0xFF);
    SSD1963_WriteData(y1 >> 8); SSD1963_WriteData(y1 & 0xFF);
    SSD1963_WriteCommand(SSD1963_WRITE_MEMORY_START);
}

void SSD1963_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if ((x + w) > LCD_WIDTH) w = LCD_WIDTH - x;
    if ((y + h) > LCD_HEIGHT) h = LCD_HEIGHT - y;

    SSD1963_SetWindow(x, y, x + w - 1, y + h - 1);
    uint32_t pixels = (uint32_t)w * (uint32_t)h;

    TFT_CS_LOW(); TFT_RS_HIGH();
    TFT_WRITE_BUS(color);
    while (pixels--) {
        TFT_WR_STROBE();
    }
    TFT_CS_HIGH();
}

void hal_display_set_brightness(uint8_t pct) {
    s_brightness = (pct > 100) ? 100 : pct;
    uint8_t pwm = (s_brightness * 255) / 100;
    SSD1963_WriteCommand(SSD1963_SET_PWM_CONF);
    SSD1963_WriteData(0x06); SSD1963_WriteData(pwm); SSD1963_WriteData(0x01);
    SSD1963_WriteData(0x00); SSD1963_WriteData(0x00); SSD1963_WriteData(0x00);

    if (s_brightness > 0) {
        TFT_BLED_ON();
    } else {
        TFT_BLED_OFF();
    }
}

int hal_display_init(void) {
    /* 1. Turn on backlight line */
    TFT_BLED_ON();
    TFT_RD_HIGH();
    TFT_CS_HIGH();
    TFT_WR_HIGH();

    /* 2. Hardware reset */
    SSD1963_Reset();

    SSD1963_WriteCommand(SSD1963_SOFT_RESET); HAL_Delay(10);

    /* 3. Set PLL */
    SSD1963_WriteCommand(SSD1963_SET_PLL_MN);
    SSD1963_WriteData(0x23); SSD1963_WriteData(0x02); SSD1963_WriteData(0x04);
    SSD1963_WriteCommand(SSD1963_SET_PLL); SSD1963_WriteData(0x01); HAL_Delay(1);
    SSD1963_WriteCommand(SSD1963_SET_PLL); SSD1963_WriteData(0x03); HAL_Delay(5);
    SSD1963_WriteCommand(SSD1963_SOFT_RESET); HAL_Delay(10);

    /* 4. Pixel clock */
    SSD1963_WriteCommand(SSD1963_SET_LSHIFT_FREQ);
    SSD1963_WriteData(0x01); SSD1963_WriteData(0x33); SSD1963_WriteData(0x33);

    /* 5. LCD mode 480x272 */
    SSD1963_WriteCommand(SSD1963_SET_LCD_MODE);
    SSD1963_WriteData(0x20); SSD1963_WriteData(0x00);
    SSD1963_WriteData((LCD_WIDTH - 1) >> 8); SSD1963_WriteData((LCD_WIDTH - 1) & 0xFF);
    SSD1963_WriteData((LCD_HEIGHT - 1) >> 8); SSD1963_WriteData((LCD_HEIGHT - 1) & 0xFF);
    SSD1963_WriteData(0x00);

    /* 6. Horizontal period */
    SSD1963_WriteCommand(SSD1963_SET_HORI_PERIOD);
    SSD1963_WriteData(0x02); SSD1963_WriteData(0x0D); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x2B); SSD1963_WriteData(0x08); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x00); SSD1963_WriteData(0x00);

    /* 7. Vertical period */
    SSD1963_WriteCommand(SSD1963_SET_VERT_PERIOD);
    SSD1963_WriteData(0x01); SSD1963_WriteData(0x1E); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x0C); SSD1963_WriteData(0x0A); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x00);

    SSD1963_WriteCommand(SSD1963_SET_ADDRESS_MODE); SSD1963_WriteData(0x00); /* RGB */
    SSD1963_WriteCommand(SSD1963_SET_PIXEL_FORMAT); SSD1963_WriteData(0x55); /* 16-bit 565 */

    /* Pixel Data Interface: 16-bit 565 format */
    SSD1963_WriteCommand(0xF0); SSD1963_WriteData(0x03);

    /* PWM backlight configuration */
    SSD1963_WriteCommand(SSD1963_SET_GPIO_CONF);
    SSD1963_WriteData(0x0F); SSD1963_WriteData(0x01);
    SSD1963_WriteCommand(SSD1963_SET_PWM_CONF);
    SSD1963_WriteData(0x06); SSD1963_WriteData(0xFF); SSD1963_WriteData(0x01);
    SSD1963_WriteData(0x00); SSD1963_WriteData(0x00); SSD1963_WriteData(0x00);

    SSD1963_WriteCommand(SSD1963_EXIT_SLEEP_MODE); HAL_Delay(120);
    SSD1963_WriteCommand(SSD1963_SET_DISPLAY_ON); HAL_Delay(25);

    hal_display_set_brightness(100);
    return 0;
}

void hal_display_clear(void) {
    SSD1963_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, s_bg);
}

void hal_display_flush(const uint8_t *mono_fb) {
    if (!mono_fb) return;

    SSD1963_SetWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    TFT_CS_LOW(); TFT_RS_HIGH();

    uint32_t row_bytes = (LCD_WIDTH + 7) / 8;
    for (uint16_t y = 0; y < LCD_HEIGHT; y++) {
        const uint8_t *row = &mono_fb[y * row_bytes];
        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            bool is_ink = (row[x >> 3] & (0x80 >> (x & 7))) == 0;
            TFT_WRITE_PIXEL(is_ink ? s_fg : s_bg);
        }
    }
    TFT_CS_HIGH();
}

void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!mono_fb || w == 0 || h == 0) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    SSD1963_SetWindow(x, y, x + w - 1, y + h - 1);
    TFT_CS_LOW(); TFT_RS_HIGH();

    uint32_t row_bytes = (LCD_WIDTH + 7) / 8;
    for (uint16_t r = 0; r < h; r++) {
        const uint8_t *row = &mono_fb[(y + r) * row_bytes];
        for (uint16_t c = 0; c < w; c++) {
            uint16_t px = x + c;
            bool is_ink = (row[px >> 3] & (0x80 >> (px & 7))) == 0;
            TFT_WRITE_PIXEL(is_ink ? s_fg : s_bg);
        }
    }
    TFT_CS_HIGH();
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data) {
    if (!rgb565_data || w == 0 || h == 0) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    SSD1963_SetWindow(x, y, x + w - 1, y + h - 1);
    TFT_CS_LOW(); TFT_RS_HIGH();

    const uint16_t *pixels = (const uint16_t*)rgb565_data;
    uint32_t total = (uint32_t)w * (uint32_t)h;
    for (uint32_t i = 0; i < total; i++) {
        TFT_WRITE_PIXEL(pixels[i]);
    }
    TFT_CS_HIGH();
}

void hal_display_present(void) {
}

void hal_display_set_theme(uint16_t fg, uint16_t bg) {
    s_fg = fg;
    s_bg = bg;
}

void hal_display_sleep(void) {
    SSD1963_WriteCommand(SSD1963_SET_DISPLAY_OFF);
    TFT_BLED_OFF();
}

#endif
