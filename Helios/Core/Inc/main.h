/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file - Helios Demo for mikromedia 4
 * @note           : Pin mappings verified from mikromedia Plus STM32F7 examples
 ******************************************************************************
 */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/

/* ========================= TFT Pin Definitions ========================= */
/* 
 * Data Bus (16-bit parallel):
 * - D0-D7:  GPIOG[0:7]  (lower byte)
 * - D8-D15: GPIOE[8:15] (upper byte)
 */
#define TFT_DATA_LO_PORT    GPIOG   /* D0-D7 */
#define TFT_DATA_HI_PORT    GPIOE   /* D8-D15 */
#define TFT_DATA_LO_MASK    0x00FF  /* PG0-PG7 */
#define TFT_DATA_HI_MASK    0xFF00  /* PE8-PE15 */

/* Control Pins on Port F */
#define TFT_BLED_PIN        GPIO_PIN_10  /* Backlight enable */
#define TFT_BLED_PORT       GPIOF
#define TFT_WR_PIN          GPIO_PIN_11
#define TFT_WR_PORT         GPIOF
#define TFT_RD_PIN          GPIO_PIN_12
#define TFT_RD_PORT         GPIOF
#define TFT_CS_PIN          GPIO_PIN_13
#define TFT_CS_PORT         GPIOF
#define TFT_RST_PIN         GPIO_PIN_14
#define TFT_RST_PORT        GPIOF
#define TFT_RS_PIN          GPIO_PIN_15  /* Data/Command select */
#define TFT_RS_PORT         GPIOF

/* ========================= Touch Pin Definitions ========================= */
/* I2C1: PB6 (SCL), PB7 (SDA) - STMPE610 resistive touch */
#define TOUCH_SCL_PIN       GPIO_PIN_6
#define TOUCH_SDA_PIN       GPIO_PIN_7
#define TOUCH_I2C_PORT      GPIOB
#define TOUCH_INT_PIN       GPIO_PIN_0
#define TOUCH_INT_PORT      GPIOA

/* ========================= Display Parameters ========================= */
#define TFT_WIDTH           480
#define TFT_HEIGHT          272

/* Exported functions prototypes ---------------------------------------------*/
extern ADC_HandleTypeDef hadc3;
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart6;

uint16_t Read_ADC3_Channel(uint32_t channel);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
