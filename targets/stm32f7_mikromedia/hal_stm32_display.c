#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_display.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ==============================================================================
// STM32F746 Peripheral Registers (Bare-Metal Direct Register Map)
// ==============================================================================
#define RCC_AHB1ENR_REG     (*((volatile uint32_t*)0x40023830))

// GPIOE (Data Port: PE0..PE15 - 16-bit Parallel Bus)
#define GPIOE_MODER_REG     (*((volatile uint32_t*)0x40021000))
#define GPIOE_OSPEEDR_REG   (*((volatile uint32_t*)0x40021008))
#define GPIOE_ODR_REG       (*((volatile uint32_t*)0x40021014))

// GPIOF (Control Port: PF10..PF15)
//   PF10: TFT_BLED (Backlight PWM Enable)
//   PF11: TFT_WR   (Write Strobe, Active Low)
//   PF12: TFT_RD   (Read Strobe, Active Low)
//   PF13: TFT_CS   (Chip Select, Active Low)
//   PF14: TFT_RST  (Reset, Active Low)
//   PF15: TFT_RS   (Register Select: 0 = Cmd, 1 = Data)
#define GPIOF_MODER_REG     (*((volatile uint32_t*)0x40021400))
#define GPIOF_OSPEEDR_REG   (*((volatile uint32_t*)0x40021408))
#define GPIOF_ODR_REG       (*((volatile uint32_t*)0x40021414))
#define GPIOF_BSRR_REG      (*((volatile uint32_t*)0x40021418))

// Inline Pin Macros using atomic BSRR
#define PIN_BLED_HIGH()     (GPIOF_BSRR_REG = (1UL << 10))
#define PIN_BLED_LOW()      (GPIOF_BSRR_REG = (1UL << (10 + 16)))

#define PIN_WR_HIGH()       (GPIOF_BSRR_REG = (1UL << 11))
#define PIN_WR_LOW()        (GPIOF_BSRR_REG = (1UL << (11 + 16)))

#define PIN_RD_HIGH()       (GPIOF_BSRR_REG = (1UL << 12))
#define PIN_RD_LOW()        (GPIOF_BSRR_REG = (1UL << (12 + 16)))

#define PIN_CS_HIGH()       (GPIOF_BSRR_REG = (1UL << 13))
#define PIN_CS_LOW()        (GPIOF_BSRR_REG = (1UL << (13 + 16)))

#define PIN_RST_HIGH()      (GPIOF_BSRR_REG = (1UL << 14))
#define PIN_RST_LOW()       (GPIOF_BSRR_REG = (1UL << (14 + 16)))

#define PIN_RS_DATA()       (GPIOF_BSRR_REG = (1UL << 15))
#define PIN_RS_CMD()        (GPIOF_BSRR_REG = (1UL << (15 + 16)))

static uint16_t s_fg = 0xFFFF;
static uint16_t s_bg = 0x0000;
static uint8_t s_brightness = 100;

static void delay_cycles(volatile uint32_t count) {
    while (count--) {
        __asm volatile("nop");
    }
}

static inline void ssd1963_write_cmd(uint8_t cmd) {
    PIN_RS_CMD();
    PIN_CS_LOW();
    GPIOE_ODR_REG = (uint16_t)cmd;
    PIN_WR_LOW();
    __asm volatile("nop; nop");
    PIN_WR_HIGH();
    PIN_CS_HIGH();
}

static inline void ssd1963_write_data(uint8_t data) {
    PIN_RS_DATA();
    PIN_CS_LOW();
    GPIOE_ODR_REG = (uint16_t)data;
    PIN_WR_LOW();
    __asm volatile("nop; nop");
    PIN_WR_HIGH();
    PIN_CS_HIGH();
}

static inline void ssd1963_write_data16(uint16_t data) {
    PIN_RS_DATA();
    PIN_CS_LOW();
    GPIOE_ODR_REG = data;
    PIN_WR_LOW();
    __asm volatile("nop");
    PIN_WR_HIGH();
    PIN_CS_HIGH();
}

static void ssd1963_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ssd1963_write_cmd(0x2A); // Set Column Address
    ssd1963_write_data((uint8_t)(x0 >> 8));
    ssd1963_write_data((uint8_t)(x0 & 0xFF));
    ssd1963_write_data((uint8_t)(x1 >> 8));
    ssd1963_write_data((uint8_t)(x1 & 0xFF));

    ssd1963_write_cmd(0x2B); // Set Page Address
    ssd1963_write_data((uint8_t)(y0 >> 8));
    ssd1963_write_data((uint8_t)(y0 & 0xFF));
    ssd1963_write_data((uint8_t)(y1 >> 8));
    ssd1963_write_data((uint8_t)(y1 & 0xFF));
}

int hal_display_init(void) {
    // 1. Enable AHB1 GPIO clocks (GPIOA..GPIOG)
    RCC_AHB1ENR_REG |= 0x7F;

    // 2. Configure GPIOE (PE0..PE15) as 16-bit High Speed Push-Pull Outputs
    GPIOE_MODER_REG = 0x55555555;   // Output mode for all 16 pins
    GPIOE_OSPEEDR_REG = 0xFFFFFFFF; // High speed

    // 3. Configure GPIOF (PF10..PF15) as High Speed Push-Pull Outputs
    GPIOF_MODER_REG &= ~0xFFF00000;
    GPIOF_MODER_REG |= 0x55500000;   // Output mode for PF10..PF15
    GPIOF_OSPEEDR_REG |= 0xFFF00000; // High speed

    // 4. Initial Pin States
    PIN_CS_HIGH();
    PIN_WR_HIGH();
    PIN_RD_HIGH();
    PIN_RS_DATA();
    PIN_BLED_HIGH(); // Turn backlight on

    // 5. Hardware Reset Pulse
    PIN_RST_LOW();
    delay_cycles(200000);
    PIN_RST_HIGH();
    delay_cycles(500000);

    // 6. SSD1963 Initialization Sequence for 4.3" 480x272 Panel
    ssd1963_write_cmd(0xE2); // Set PLL MN
    ssd1963_write_data(0x2D); // M = 45 -> PLL = 10 * 45 = 450 MHz
    ssd1963_write_data(0x02); // N = 2 -> VCO = 450 / (2 + 1) = 150 MHz
    ssd1963_write_data(0x04); // Enable PLL

    ssd1963_write_cmd(0xE0); // Start PLL
    ssd1963_write_data(0x01);
    delay_cycles(20000);

    ssd1963_write_cmd(0xE0); // Lock PLL
    ssd1963_write_data(0x03);
    delay_cycles(50000);

    ssd1963_write_cmd(0x01); // Software Reset
    delay_cycles(100000);

    // Set Pixel Clock (LSHIFT = 9 MHz for 480x272 @ 60Hz)
    // 9 MHz / 110 MHz * 2^20 = 0x00FFBE
    ssd1963_write_cmd(0xE6);
    ssd1963_write_data(0x00);
    ssd1963_write_data(0xFF);
    ssd1963_write_data(0xBE);

    // Set Panel Mode (24-bit TFT, Hsync/Vsync low active, 480x272)
    ssd1963_write_cmd(0xB0);
    ssd1963_write_data(0x20); // 24-bit panel, FRC, dither
    ssd1963_write_data(0x00); // TFT mode
    ssd1963_write_data(0x01); // Horizontal size = 480 - 1 = 479 (0x01DF) high byte
    ssd1963_write_data(0xDF); // low byte
    ssd1963_write_data(0x01); // Vertical size = 272 - 1 = 271 (0x010F) high byte
    ssd1963_write_data(0x0F); // low byte
    ssd1963_write_data(0x00); // RGB order

    // Set Horizontal Period
    ssd1963_write_cmd(0xB4);
    ssd1963_write_data(0x02); // HT (Horizontal total period = 525) high byte
    ssd1963_write_data(0x0D); // low byte
    ssd1963_write_data(0x00); // HPS (Horizontal pulse start = 43) high byte
    ssd1963_write_data(0x2B); // low byte
    ssd1963_write_data(0x28); // HPW (Horizontal sync pulse width = 40)
    ssd1963_write_data(0x00); // LPS (Horizontal sync pulse start = 0)
    ssd1963_write_data(0x00);
    ssd1963_write_data(0x00);

    // Set Vertical Period
    ssd1963_write_cmd(0xB6);
    ssd1963_write_data(0x01); // VT (Vertical total period = 286) high byte
    ssd1963_write_data(0x1E); // low byte
    ssd1963_write_data(0x00); // VPS (Vertical pulse start = 12) high byte
    ssd1963_write_data(0x0C); // low byte
    ssd1963_write_data(0x09); // VPW (Vertical pulse width = 9)
    ssd1963_write_data(0x00); // FPS (Vertical sync pulse start = 0)
    ssd1963_write_data(0x00);

    // Set Pixel Data Interface (16-bit 565 format)
    ssd1963_write_cmd(0xF0);
    ssd1963_write_data(0x03); // 16-bit 565

    // Set Address Mode (normal orientation)
    ssd1963_write_cmd(0x36);
    ssd1963_write_data(0x00);

    // Turn Display ON
    ssd1963_write_cmd(0x29);

    // Clear Screen to Background Color
    hal_display_clear();

    return 0;
}

void hal_display_clear(void) {
    ssd1963_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    ssd1963_write_cmd(0x2C); // Write Memory Start

    PIN_RS_DATA();
    PIN_CS_LOW();
    GPIOE_ODR_REG = s_bg;
    for (uint32_t i = 0; i < (uint32_t)(LCD_WIDTH * LCD_HEIGHT); i++) {
        PIN_WR_LOW();
        __asm volatile("nop");
        PIN_WR_HIGH();
    }
    PIN_CS_HIGH();
}

void hal_display_flush(const uint8_t *mono_fb) {
    if (!mono_fb) return;

    ssd1963_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    ssd1963_write_cmd(0x2C); // Write Memory Start

    PIN_RS_DATA();
    PIN_CS_LOW();

    uint32_t row_bytes = (LCD_WIDTH + 7) / 8;
    for (uint16_t y = 0; y < LCD_HEIGHT; y++) {
        const uint8_t *row = &mono_fb[y * row_bytes];
        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            bool pixel = (row[x >> 3] & (0x80 >> (x & 7))) != 0;
            GPIOE_ODR_REG = pixel ? s_fg : s_bg;
            PIN_WR_LOW();
            __asm volatile("nop");
            PIN_WR_HIGH();
        }
    }

    PIN_CS_HIGH();
}

void hal_display_flush_region(const uint8_t *mono_fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!mono_fb || w == 0 || h == 0) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    ssd1963_set_window(x, y, x + w - 1, y + h - 1);
    ssd1963_write_cmd(0x2C);

    PIN_RS_DATA();
    PIN_CS_LOW();

    uint32_t row_bytes = (LCD_WIDTH + 7) / 8;
    for (uint16_t r = 0; r < h; r++) {
        const uint8_t *row = &mono_fb[(y + r) * row_bytes];
        for (uint16_t c = 0; c < w; c++) {
            uint16_t px = x + c;
            bool pixel = (row[px >> 3] & (0x80 >> (px & 7))) != 0;
            GPIOE_ODR_REG = pixel ? s_fg : s_bg;
            PIN_WR_LOW();
            __asm volatile("nop");
            PIN_WR_HIGH();
        }
    }

    PIN_CS_HIGH();
}

void hal_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565_data) {
    if (!rgb565_data || w == 0 || h == 0) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    ssd1963_set_window(x, y, x + w - 1, y + h - 1);
    ssd1963_write_cmd(0x2C);

    PIN_RS_DATA();
    PIN_CS_LOW();

    const uint16_t *pixels = (const uint16_t*)rgb565_data;
    uint32_t total = (uint32_t)w * (uint32_t)h;
    for (uint32_t i = 0; i < total; i++) {
        GPIOE_ODR_REG = pixels[i];
        PIN_WR_LOW();
        __asm volatile("nop");
        PIN_WR_HIGH();
    }

    PIN_CS_HIGH();
}

void hal_display_present(void) {
}

void hal_display_set_brightness(uint8_t pct) {
    s_brightness = (pct > 100) ? 100 : pct;
    if (s_brightness > 0) {
        PIN_BLED_HIGH();
    } else {
        PIN_BLED_LOW();
    }
}

void hal_display_set_theme(uint16_t fg, uint16_t bg) {
    s_fg = fg;
    s_bg = bg;
}

void hal_display_sleep(void) {
    ssd1963_write_cmd(0x28); // Display OFF
    PIN_BLED_LOW();          // Backlight OFF
}

#endif
