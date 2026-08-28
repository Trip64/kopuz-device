#ifndef __STMPE610_H
#define __STMPE610_H

#include "stm32f7xx_hal.h"

/* STMPE610 Registers */
#define STMPE610_CHIP_ID        0x00
#define STMPE610_ID_VER         0x02
#define STMPE610_SYS_CTRL1      0x03
#define STMPE610_SYS_CTRL2      0x04
#define STMPE610_SPI_CFG        0x08
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
#define STMPE610_TSC_SHIELD     0x59

/* Functions */
uint8_t STMPE610_Init(I2C_HandleTypeDef *hi2c, uint16_t address);
uint8_t STMPE610_ReadXYZ(uint16_t *x, uint16_t *y, uint8_t *z);
uint8_t STMPE610_Touched(void);

#endif /* __STMPE610_H */
