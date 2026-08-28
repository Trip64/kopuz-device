#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_input.h"
#include "stm32f7xx_hal.h"
#include "stmpe610.h"
#include <stdbool.h>

static I2C_HandleTypeDef s_hi2c1;
static UART_HandleTypeDef s_huart6;
static uint8_t s_touch_found = 0;
static uint32_t s_last_touch_time = 0;

void hal_input_init(void) {
    // 1. Initialize I2C1 for STMPE610 Touch Controller (exact Helios pinout)
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
    s_hi2c1.Init.Timing = 0x40912732; // 400kHz @ 54MHz PCLK1
    s_hi2c1.Init.OwnAddress1 = 0;
    s_hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    s_hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    s_hi2c1.Init.OwnAddress2 = 0;
    s_hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    s_hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    s_hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&s_hi2c1);

    HAL_I2CEx_ConfigAnalogFilter(&s_hi2c1, I2C_ANALOGFILTER_ENABLE);
    HAL_I2CEx_ConfigDigitalFilter(&s_hi2c1, 0);

    // Try both standard addresses
    s_touch_found = STMPE610_Init(&s_hi2c1, 0x41 << 1);
    if (!s_touch_found) {
        s_touch_found = STMPE610_Init(&s_hi2c1, 0x44 << 1);
    }
    if (!s_touch_found) {
        s_touch_found = STMPE610_Init(&s_hi2c1, 0x82);
    }

    // 2. Initialize USART6 (PC6 TX / PC7 RX)
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
    // 1. Check UART RX
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
                return BTN_VOL_DOWN;
            case 'D':
            case 'F':
                extern void hal_system_enter_dfu(void);
                hal_system_enter_dfu();
                break;
            case 'm':
            case 'M':
            case 27: // ESC
            case '\b':
                return BTN_BACK;
            default:
                break;
        }
    }

    // 2. Check STMPE610 Touch using exact Helios routines
    if (s_touch_found && STMPE610_Touched()) {
        uint16_t x, y;
        uint8_t z;
        if (STMPE610_ReadXYZ(&x, &y, &z)) {
            uint32_t now = HAL_GetTick();
            if (now - s_last_touch_time > 300) {
                s_last_touch_time = now;

                // Helios calibrated mapping
                int px = (x > 250) ? ((x - 250) * 480 / 3550) : 0;
                int py = (y > 250) ? ((y - 250) * 272 / 3550) : 0;

                // Visual feedback: toggle blue LED on touch
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4);

                if (py < 45) {
                    return BTN_BACK; // Top tab/header -> Menu / Back
                } else if (px < 160) {
                    return BTN_PREV; // Left region -> Previous track / Up
                } else if (px > 320) {
                    return BTN_NEXT; // Right region -> Next track / Down
                } else {
                    return BTN_PLAY_PAUSE; // Center region -> Play/Pause
                }
            }
        }
    }

    return BTN_NONE;
}

#endif
