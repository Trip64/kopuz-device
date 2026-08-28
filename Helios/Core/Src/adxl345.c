#include "adxl345.h"

static const uint8_t ADXL345_ID = 0xE5;

/**
 * @brief  Initialize ADXL345
 */
HAL_StatusTypeDef ADXL345_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id = 0;
    uint8_t data[2];
    
    /* Check Device ID */
    HAL_I2C_Mem_Read(hi2c, ADXL345_ADDR, ADXL345_REG_DEVID, I2C_MEMADD_SIZE_8BIT, &id, 1, 100);
    if (id != ADXL345_ID) {
        return HAL_ERROR;
    }
    
    /* Power Control: Standby */
    data[0] = 0x00;
    HAL_I2C_Mem_Write(hi2c, ADXL345_ADDR, ADXL345_REG_POWER_CTL, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
    
    /* Data Format: Full Res, +/- 2g */
    /* 0x08 = Full Res, Range 00 (+/- 2g) */
    data[0] = 0x08;
    HAL_I2C_Mem_Write(hi2c, ADXL345_ADDR, ADXL345_REG_DATA_FORMAT, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
    
    /* FIFO Control: Stream mode */
    data[0] = 0x80; // Stream
    HAL_I2C_Mem_Write(hi2c, ADXL345_ADDR, ADXL345_REG_FIFO_CTL, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
    
    /* Power Control: Measurement Mode */
    data[0] = 0x08; // Measure (Bit 3)
    HAL_I2C_Mem_Write(hi2c, ADXL345_ADDR, ADXL345_REG_POWER_CTL, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
    
    return HAL_OK;
}

/**
 * @brief  Read XYZ Acceleration
 */
void ADXL345_ReadXYZ(I2C_HandleTypeDef *hi2c, int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t data[6];
    
    /* Read 6 bytes starting from DATAX0 */
    HAL_I2C_Mem_Read(hi2c, ADXL345_ADDR, ADXL345_REG_DATAX0, I2C_MEMADD_SIZE_8BIT, data, 6, 100);
    
    *x = (int16_t)((data[1] << 8) | data[0]);
    *y = (int16_t)((data[3] << 8) | data[2]);
    *z = (int16_t)((data[5] << 8) | data[4]);
}
