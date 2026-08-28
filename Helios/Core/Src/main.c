/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : SOLARIS Dashboard - mikromedia Plus STM32F7
 * @note           : Pin mappings verified from mikromedia Plus STM32F7 examples
 ******************************************************************************
 */
#include "main.h"
#include "ssd1963.h"
#include "graphics.h"
#include "dashboard.h"
#include "uart_rx.h"

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart6;
ADC_HandleTypeDef hadc3;
int __errno; /* Fix for sqrtf */

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_ADC3_Init(void);

/**
 * @brief  Main
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC3_Init();
    MX_I2C1_Init();
    MX_USART6_UART_Init();
    
    /* Init Display */
    SSD1963_Init();
    SSD1963_SetBacklight(100);
    
    /* Start Dashboard */
    UART_RX_Init(&huart6);
    Dashboard_Init(&hi2c1);
    Dashboard_Run();
    
    /* Should not reach here */
    while (1) {}
}

/**
 * @brief  System Clock Configuration
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 432;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
    
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) Error_Handler();
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK) Error_Handler();
    
    /* I2C1 + USART2 Clock Sources */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_USART2;
    PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    PeriphClkInitStruct.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) Error_Handler();
}

/**
 * @brief  I2C1 Init (PB6/PB7)
 */
static void MX_I2C1_Init(void)
{
    /* Enable Peripheral Clock */
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    hi2c1.Instance = I2C1;
    /* Timing for 400kHz @ 54MHz PCLK1 (CubeMX calculated) */
    hi2c1.Init.Timing = 0x40912732; 
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
    
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) Error_Handler();
}

/**
 * @brief  USART6 Init - Raspberry Pi Gateway connection
 *         Default pins: PC6 (TX), PC7 (RX)
 *         Baud: 115200, 8N1
 */
static void MX_USART6_UART_Init(void)
{
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    /* USART6 GPIO: PC6=TX, PC7=RX */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) Error_Handler();

    /* Enable USART6 IRQ for receiving telemetry */
    HAL_NVIC_SetPriority(USART6_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
}

/**
 * @brief  GPIO Initialization
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    /* Touch Interrupt Pin: PA0 */
    GPIO_InitStruct.Pin = TOUCH_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_INT_PORT, &GPIO_InitStruct);
    
    /* ADC3 Analog Inputs: PF8 (IN6/Temp), PF9 (IN7/Light) */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
    
    /* I2C1 GPIO Configuration: PB6=SCL, PB7=SDA */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* TFT Data Bus: PE8-PE15 */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                          GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* TFT Data Bus: PG0-PG7 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    /* TFT Control Pins on Port F */
    GPIO_InitStruct.Pin = TFT_BLED_PIN | TFT_WR_PIN | TFT_RD_PIN | TFT_CS_PIN | TFT_RST_PIN | TFT_RS_PIN;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
    
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_RD_PORT, TFT_RD_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_WR_PORT, TFT_WR_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_BLED_PORT, TFT_BLED_PIN, GPIO_PIN_SET);
}

/**
 * @brief ADC3 Initialization
 */
static void MX_ADC3_Init(void)
{
    __HAL_RCC_ADC3_CLK_ENABLE();
    
    hadc3.Instance = ADC3;
    hadc3.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
    hadc3.Init.Resolution = ADC_RESOLUTION_12B;
    hadc3.Init.ScanConvMode = DISABLE;
    hadc3.Init.ContinuousConvMode = DISABLE;
    hadc3.Init.DiscontinuousConvMode = DISABLE;
    hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc3.Init.NbrOfConversion = 1;
    hadc3.Init.DMAContinuousRequests = DISABLE;
    hadc3.Init.EOCSelection = EOC_SINGLE_CONV;
    
    if (HAL_ADC_Init(&hadc3) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief Polled ADC Channel Read for Sensors
 */
uint16_t Read_ADC3_Channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    
    if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
        return 9991; /* Error code 1 */
    }
    
    HAL_ADC_Start(&hadc3);
    if (HAL_ADC_PollForConversion(&hadc3, 100) == HAL_OK) {
        uint16_t val = HAL_ADC_GetValue(&hadc3);
        HAL_ADC_Stop(&hadc3);
        return val;
    }
    HAL_ADC_Stop(&hadc3);
    return 9992; /* Error code 2 (Timeout) */
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
