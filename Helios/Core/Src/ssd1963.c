/**
 ******************************************************************************
 * @file           : ssd1963.c
 * @brief          : SSD1963 TFT Display Driver Implementation
 * @note           : Pin mappings verified from mikromedia Plus STM32F7 examples
 *                   Data bus: GPIOG[0:7] = D0-D7, GPIOE[8:15] = D8-D15
 ******************************************************************************
 */
#include "ssd1963.h"
#include "main.h"

/* Control pin macros for fast GPIO access */
#define TFT_CS_LOW()    (TFT_CS_PORT->BSRR = (uint32_t)TFT_CS_PIN << 16)
#define TFT_CS_HIGH()   (TFT_CS_PORT->BSRR = TFT_CS_PIN)
#define TFT_RS_LOW()    (TFT_RS_PORT->BSRR = (uint32_t)TFT_RS_PIN << 16)
#define TFT_RS_HIGH()   (TFT_RS_PORT->BSRR = TFT_RS_PIN)
#define TFT_WR_LOW()    (TFT_WR_PORT->BSRR = (uint32_t)TFT_WR_PIN << 16)
#define TFT_WR_HIGH()   (TFT_WR_PORT->BSRR = TFT_WR_PIN)
#define TFT_RD_LOW()    (TFT_RD_PORT->BSRR = (uint32_t)TFT_RD_PIN << 16)
#define TFT_RD_HIGH()   (TFT_RD_PORT->BSRR = TFT_RD_PIN)
#define TFT_RST_LOW()   (TFT_RST_PORT->BSRR = (uint32_t)TFT_RST_PIN << 16)
#define TFT_RST_HIGH()  (TFT_RST_PORT->BSRR = TFT_RST_PIN)
#define TFT_BLED_ON()   (TFT_BLED_PORT->BSRR = TFT_BLED_PIN)
#define TFT_BLED_OFF()  (TFT_BLED_PORT->BSRR = (uint32_t)TFT_BLED_PIN << 16)

/* Write 16-bit data to split data bus: GPIOG[0:7] + GPIOE[8:15] */
static inline void TFT_WRITE_BUS(uint16_t data) {
    uint32_t temp;
    /* Write high byte to GPIOE[8:15] */
    temp = TFT_DATA_HI_PORT->ODR;
    temp &= ~TFT_DATA_HI_MASK;
    TFT_DATA_HI_PORT->ODR = temp | ((data & 0xFF00));
    /* Write low byte to GPIOG[0:7] */
    temp = TFT_DATA_LO_PORT->ODR;
    temp &= ~TFT_DATA_LO_MASK;
    TFT_DATA_LO_PORT->ODR = temp | (data & 0x00FF);
}

#define TFT_WR_STROBE() do { TFT_WR_LOW(); __NOP(); TFT_WR_HIGH(); } while(0)

static uint8_t backlight_level = 100;

void SSD1963_Reset(void) {
    TFT_RST_HIGH(); HAL_Delay(5);
    TFT_RST_LOW();  HAL_Delay(20);
    TFT_RST_HIGH(); HAL_Delay(150);
}

void SSD1963_WriteCommand(uint8_t cmd) {
    TFT_CS_LOW(); TFT_RS_LOW();
    TFT_WRITE_BUS(cmd);
    TFT_WR_STROBE();
    TFT_CS_HIGH();
}

void SSD1963_WriteData(uint8_t data) {
    TFT_CS_LOW(); TFT_RS_HIGH();
    TFT_WRITE_BUS(data);
    TFT_WR_STROBE();
    TFT_CS_HIGH();
}

void SSD1963_WriteData16(uint16_t data) {
    TFT_CS_LOW(); TFT_RS_HIGH();
    TFT_WRITE_BUS(data);
    TFT_WR_STROBE();
    TFT_CS_HIGH();
}

void SSD1963_Init(void) {
    /* Turn on backlight first */
    TFT_BLED_ON();
    
    /* Hardware reset */
    SSD1963_Reset();
    
    SSD1963_WriteCommand(SSD1963_SOFT_RESET); HAL_Delay(10);
    
    /* Set PLL */
    SSD1963_WriteCommand(SSD1963_SET_PLL_MN);
    SSD1963_WriteData(0x23); SSD1963_WriteData(0x02); SSD1963_WriteData(0x04);
    SSD1963_WriteCommand(SSD1963_SET_PLL); SSD1963_WriteData(0x01); HAL_Delay(1);
    SSD1963_WriteCommand(SSD1963_SET_PLL); SSD1963_WriteData(0x03); HAL_Delay(5);
    SSD1963_WriteCommand(SSD1963_SOFT_RESET); HAL_Delay(10);
    
    /* Pixel clock */
    SSD1963_WriteCommand(SSD1963_SET_LSHIFT_FREQ);
    SSD1963_WriteData(0x01); SSD1963_WriteData(0x33); SSD1963_WriteData(0x33);
    
    /* LCD mode 480x272 */
    SSD1963_WriteCommand(SSD1963_SET_LCD_MODE);
    SSD1963_WriteData(0x20); SSD1963_WriteData(0x00);
    SSD1963_WriteData((TFT_WIDTH-1)>>8); SSD1963_WriteData((TFT_WIDTH-1)&0xFF);
    SSD1963_WriteData((TFT_HEIGHT-1)>>8); SSD1963_WriteData((TFT_HEIGHT-1)&0xFF);
    SSD1963_WriteData(0x00);
    
    /* Horizontal period */
    SSD1963_WriteCommand(SSD1963_SET_HORI_PERIOD);
    SSD1963_WriteData(0x02); SSD1963_WriteData(0x0D); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x2B); SSD1963_WriteData(0x08); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x00); SSD1963_WriteData(0x00);
    
    /* Vertical period */
    SSD1963_WriteCommand(SSD1963_SET_VERT_PERIOD);
    SSD1963_WriteData(0x01); SSD1963_WriteData(0x1E); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x0C); SSD1963_WriteData(0x0A); SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x00);
    
    SSD1963_WriteCommand(SSD1963_SET_ADDRESS_MODE); SSD1963_WriteData(0x00); /* RGB */
    SSD1963_WriteCommand(SSD1963_SET_PIXEL_FORMAT); SSD1963_WriteData(0x55); /* 16-bit 565 */
    
    /* Pixel Data Interface: 16-bit 565 format */
    SSD1963_WriteCommand(0xF0); SSD1963_WriteData(0x03);
    
    /* PWM backlight via SSD1963 */
    SSD1963_WriteCommand(SSD1963_SET_GPIO_CONF);
    SSD1963_WriteData(0x0F); SSD1963_WriteData(0x01);
    SSD1963_WriteCommand(SSD1963_SET_PWM_CONF);
    SSD1963_WriteData(0x06); SSD1963_WriteData(0xFF); SSD1963_WriteData(0x01);
    SSD1963_WriteData(0x00); SSD1963_WriteData(0x00); SSD1963_WriteData(0x00);
    
    SSD1963_WriteCommand(SSD1963_EXIT_SLEEP_MODE); HAL_Delay(120);
    SSD1963_WriteCommand(SSD1963_SET_DISPLAY_ON); HAL_Delay(25);
    
    SSD1963_FillScreen(COLOR_BLACK);
}

void SSD1963_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    SSD1963_WriteCommand(SSD1963_SET_COLUMN_ADDRESS);
    SSD1963_WriteData(x0>>8); SSD1963_WriteData(x0&0xFF);
    SSD1963_WriteData(x1>>8); SSD1963_WriteData(x1&0xFF);
    SSD1963_WriteCommand(SSD1963_SET_PAGE_ADDRESS);
    SSD1963_WriteData(y0>>8); SSD1963_WriteData(y0&0xFF);
    SSD1963_WriteData(y1>>8); SSD1963_WriteData(y1&0xFF);
    SSD1963_WriteCommand(SSD1963_WRITE_MEMORY_START);
}

void SSD1963_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    SSD1963_SetWindow(x, y, x, y);
    SSD1963_WriteData16(color);
}

void SSD1963_FillScreen(uint16_t color) {
    SSD1963_FillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void SSD1963_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    if ((x + w) > TFT_WIDTH) w = TFT_WIDTH - x;
    if ((y + h) > TFT_HEIGHT) h = TFT_HEIGHT - y;
    
    SSD1963_SetWindow(x, y, x + w - 1, y + h - 1);
    uint32_t pixels = (uint32_t)w * (uint32_t)h;
    
    TFT_CS_LOW(); TFT_RS_HIGH();
    TFT_WRITE_BUS(color);
    while (pixels--) { TFT_WR_STROBE(); }
    TFT_CS_HIGH();
}

void SSD1963_SetBacklight(uint8_t level) {
    if (level > 100) level = 100;
    backlight_level = level;
    uint8_t pwm = (level * 255) / 100;
    SSD1963_WriteCommand(SSD1963_SET_PWM_CONF);
    SSD1963_WriteData(0x06); SSD1963_WriteData(pwm); SSD1963_WriteData(0x01);
    SSD1963_WriteData(0x00); SSD1963_WriteData(0x00); SSD1963_WriteData(0x00);
    
    /* Also control GPIO backlight enable */
    if (level > 0) {
        TFT_BLED_ON();
    } else {
        TFT_BLED_OFF();
    }
}
