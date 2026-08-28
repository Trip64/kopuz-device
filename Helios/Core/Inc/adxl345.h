#ifndef __ADXL345_H
#define __ADXL345_H

#include "stm32f7xx_hal.h"

/* ADXL345 Registers */
#define ADXL345_REG_DEVID          0x00
#define ADXL345_REG_POWER_CTL      0x2D
#define ADXL345_REG_DATA_FORMAT    0x31
#define ADXL345_REG_FIFO_CTL       0x38
#define ADXL345_REG_DATAX0         0x32

/* ADXL345 Address (ALT=0) */
#define ADXL345_ADDR               (0x53 << 1)

/* Functions */
HAL_StatusTypeDef ADXL345_Init(I2C_HandleTypeDef *hi2c);
void ADXL345_ReadXYZ(I2C_HandleTypeDef *hi2c, int16_t *x, int16_t *y, int16_t *z);

#endif /* __ADXL345_H */
