#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_input.h"
#include "stm32f7xx_hal.h"
#include <stdbool.h>

// ==============================================================================
// STMPE610 Resistive Touch Controller (over I2C1: PB6=SCL, PB7=SDA, Addr=0x82)
// ==============================================================================
#define STMPE610_ADDR           0x82
#define STMPE610_SYS_CTRL1      0x03
#define STMPE610_SYS_CTRL2      0x04
#define STMPE610_INT_CTRL       0x09
#define STMPE610_INT_EN         0x0A
#define STMPE610_INT_STA        0x0B
#define STMPE610_ADC_CTRL1      0x20
#define STMPE610_ADC_CTRL2      0x21
#define STMPE610_TSC_CTRL       0x40
#define STMPE610_TSC_CFG        0x41
#define STMPE610_FIFO_TH        0x4A
#define STMPE610_FIFO_STA       0x4B
#define STMPE610_FIFO_SIZE      0x4C
#define STMPE610_TSC_DATA_X     0x4D
#define STMPE610_TSC_DATA_Y     0x4F
#define STMPE610_TSC_DATA_Z     0x51
#define STMPE610_TSC_FRACTION_Z 0x56
#define STMPE610_TSC_I_DRIVE    0x58

static I2C_HandleTypeDef s_hi2c1;
static UART_HandleTypeDef s_huart6;
static bool s_touch_ok = false;
static uint32_t s_last_touch_ms = 0;

static void i2c_write_reg(uint8_t reg, uint8_t val) {
    HAL_I2C_Mem_Write(&s_hi2c1, STMPE610_ADDR, reg, 1, &val, 1, 50);
}

static uint8_t i2c_read_reg(uint8_t reg) {
    uint8_t val = 0;
    HAL_I2C_Mem_Read(&s_hi2c1, STMPE610_ADDR, reg, 1, &val, 1, 50);
    return val;
}

void hal_input_init(void) {
    // 1. Initialize I2C1 for STMPE610 Touch Controller
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    s_hi2c1.Instance = I2C1;
    s_hi2c1.Init.Timing = 0x40912732; // 400 kHz @ 54 MHz PCLK1
    s_hi2c1.Init.OwnAddress1 = 0;
    s_hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    s_hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    s_hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    s_hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&s_hi2c1);

    // Initialize STMPE610
    if (HAL_I2C_IsDeviceReady(&s_hi2c1, STMPE610_ADDR, 2, 50) == HAL_OK) {
        i2c_write_reg(STMPE610_SYS_CTRL1, 0x02); // Reset
        HAL_Delay(10);
        i2c_write_reg(STMPE610_SYS_CTRL1, 0x00);
        i2c_write_reg(STMPE610_ADC_CTRL1, 0x49); // 10-bit ADC
        HAL_Delay(2);
        i2c_write_reg(STMPE610_SYS_CTRL2, 0x04 | 0x08); // Enable ADC & TSC
        i2c_write_reg(STMPE610_ADC_CTRL2, 0x02);
        i2c_write_reg(STMPE610_TSC_CFG, 0xA0 | 0x20 | 0x04); // 8-sample avg
        i2c_write_reg(STMPE610_TSC_FRACTION_Z, 0x07);
        i2c_write_reg(STMPE610_FIFO_TH, 1);
        i2c_write_reg(STMPE610_FIFO_STA, 0x01);
        i2c_write_reg(STMPE610_FIFO_STA, 0x00);
        i2c_write_reg(STMPE610_TSC_I_DRIVE, 0x00); // 20mA drive
        i2c_write_reg(STMPE610_TSC_CTRL, 0x03);    // Enable TSC + XYZ
        s_touch_ok = true;
    }

    // 2. Initialize USART6 for Serial Command Line Interface (PC6 TX / PC7 RX)
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    s_huart6.Instance = USART6;
    s_huart6.Init.BaudRate = 115200;
    s_huart6.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart6.Init.StopBits = UART_STOPBITS_1;
    s_huart6.Init.Parity = UART_PARITY_NONE;
    s_huart6.Init.Mode = UART_MODE_TX_RX;
    s_huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&s_huart6);
}

btn_event_t hal_input_poll(void) {
    // 1. Check Serial UART input
    if (__HAL_UART_GET_FLAG(&s_huart6, UART_FLAG_RXNE)) {
        uint8_t ch = (uint8_t)(s_huart6.Instance->RDR & 0xFF);
        switch (ch) {
            case ' ':
            case 'p':
            case 'P':
            case '\r':
            case '\n':
                return BTN_PLAY_PAUSE;
            case 'n':
            case 'N':
            case 'j':
            case 'J':
            case '>':
                return BTN_NEXT;
            case 'b':
            case 'B':
            case 'k':
            case 'K':
            case '<':
                return BTN_PREV;
            case '+':
            case '=':
            case 'u':
            case 'U':
                return BTN_VOL_UP;
            case '-':
            case '_':
            case 'd':
            case 'D':
                return BTN_VOL_DOWN;
            case 'm':
            case 'M':
            case 27: // ESC
            case '\b':
                return BTN_BACK;
            default:
                break;
        }
    }

    // 2. Check STMPE610 Touch Screen
    if (s_touch_ok && (i2c_read_reg(STMPE610_TSC_CTRL) & 0x80)) {
        uint32_t now = HAL_GetTick();
        if (now - s_last_touch_ms > 250) { // Debounce
            uint8_t datax[2], datay[2];
            HAL_I2C_Mem_Read(&s_hi2c1, STMPE610_ADDR, STMPE610_TSC_DATA_X, 1, datax, 2, 20);
            HAL_I2C_Mem_Read(&s_hi2c1, STMPE610_ADDR, STMPE610_TSC_DATA_Y, 1, datay, 2, 20);

            // Reset FIFO
            i2c_write_reg(STMPE610_FIFO_STA, 0x01);
            i2c_write_reg(STMPE610_FIFO_STA, 0x00);

            uint16_t raw_x = (datax[0] << 8) | datax[1];
            uint16_t raw_y = (datay[0] << 8) | datay[1];

            int px = (raw_x > 250) ? ((raw_x - 250) * 480 / 3550) : 0;
            int py = (raw_y > 250) ? ((raw_y - 250) * 272 / 3550) : 0;

            s_last_touch_ms = now;

            // Map touch area to Kopuz controls:
            if (py < 45) {
                return BTN_BACK; // Top header tap -> Menu / Back
            } else if (px < 150) {
                return BTN_PREV; // Left side tap -> Previous track / Scroll Up
            } else if (px > 330) {
                return BTN_NEXT; // Right side tap -> Next track / Scroll Down
            } else {
                return BTN_PLAY_PAUSE; // Center tap -> Play / Pause / Select
            }
        }
    }

    return BTN_NONE;
}

#endif
