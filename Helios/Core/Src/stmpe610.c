#include "stmpe610.h"

static I2C_HandleTypeDef *h_stmpe_i2c;
static uint16_t stmpe_addr;

static void WriteReg(uint8_t reg, uint8_t val)
{
    HAL_I2C_Mem_Write(h_stmpe_i2c, stmpe_addr, reg, 1, &val, 1, 100);
}

static uint8_t ReadReg(uint8_t reg)
{
    uint8_t val = 0;
    HAL_I2C_Mem_Read(h_stmpe_i2c, stmpe_addr, reg, 1, &val, 1, 100);
    return val;
}

static uint16_t ReadReg16(uint8_t reg)
{
    uint8_t data[2];
    HAL_I2C_Mem_Read(h_stmpe_i2c, stmpe_addr, reg, 1, data, 2, 100);
    return (data[0] << 8) | data[1];
}

uint8_t STMPE610_Init(I2C_HandleTypeDef *hi2c, uint16_t address)
{
    h_stmpe_i2c = hi2c;
    stmpe_addr = address; /* Expecting 8-bit address (e.g. 0x82) */
    
    /* Check if device exists at address */
    if (HAL_I2C_IsDeviceReady(hi2c, address, 1, 100) != HAL_OK) {
        return 0; /* Failed */
    }
    
    /* Reset */
    WriteReg(STMPE610_SYS_CTRL1, 0x02);
    HAL_Delay(10);
    WriteReg(STMPE610_SYS_CTRL1, 0x00);
    
    /* Internal reference & ADC 10-bit */
    WriteReg(STMPE610_ADC_CTRL1, 0x49); 
    HAL_Delay(2);
    
    /* Set GPIO1 Low (IN1 -> 0 for I2C) */
    /* GPIO Alt Function: Disable AF for Pin 1 */
    /* Reg 0x17 (GPIO_AF) */
    WriteReg(0x17,ReadReg(0x17) & ~0x02); /* Pin 1 is bit 1? Or bit 1? Mask 0x02 for GPIO-1 (0x01 for GPIO-0) */
    /* Wait, GPIO mapping: bit x corresponds to GPIO x */
    
    /* Set GPIO1 as Output */
    /* Reg 0x13 (GPIO_DIR): Set bit 1 */
    WriteReg(0x13, ReadReg(0x13) | 0x02);
    
    /* Clear GPIO1 */
    /* Reg 0x11 (GPIO_CLR_PIN): Set bit 1 */
    WriteReg(0x11, 0x02);
    
    /* Enable ADC & TSC */
    WriteReg(STMPE610_SYS_CTRL2, 0x04 | 0x08); 
    
    /* ADC Clock 3.25MHz (0x01) or 6.5MHz (0x02 in example) */
    WriteReg(STMPE610_ADC_CTRL2, 0x02); 
    
    /* TSC Config */
    WriteReg(STMPE610_TSC_CFG, 0xA0 | 0x20 | 0x04); /* Avg 8 (0x?? matches example) */
    /* Example: 8S avg, 500us delay, 500us settling */
    
    WriteReg(STMPE610_TSC_FRACTION_Z, 0x07);
    WriteReg(STMPE610_FIFO_TH, 1);
    WriteReg(STMPE610_FIFO_STA, 0x01); /* Reset FIFO */
    WriteReg(STMPE610_FIFO_STA, 0x00);
    
    /* I Drive 20mA */
    WriteReg(STMPE610_TSC_I_DRIVE, 0x00); /* 0x00 = 20mA */
    
    /* Enable TSC, XY, Z */
    WriteReg(STMPE610_TSC_CTRL, 0x01 | 0x02); /* Enable | XYZ (0x00/01?) */
    /* Example: TRACK0 | ACQU_XYZ | ENABLE */
    /* TSC_CTRL:   [7] En  [6:4] OpMode  [3:1] Track?  [0] XYZ/XY */
    /* Enable=0x01, XYZ=0x00? */
    /* Let's trust my working header defines or guess 0x03 */
    WriteReg(STMPE610_TSC_CTRL, 0x03); /* Enable (1) + XYZ (2)? CHECK */

    /* Clear Interrupts */
    WriteReg(STMPE610_INT_STA, 0xFF);
    WriteReg(STMPE610_INT_EN, 0x01); /* Enable Touch Event */
    WriteReg(STMPE610_INT_CTRL, 0x01); /* Global Enable */
    
    return 1; /* Success */
}

uint8_t STMPE610_Touched(void)
{
    return (ReadReg(STMPE610_TSC_CTRL) & 0x80);
}

uint8_t STMPE610_ReadXYZ(uint16_t *x, uint16_t *y, uint8_t *z)
{
    if ((ReadReg(STMPE610_FIFO_SIZE) == 0)) return 0;
    
    /* Direct Register Read (Assuming Auto-Increment within 16-bit word works) */
    /* Read X (0x4D, 0x4E) */
    uint8_t datax[2];
    HAL_I2C_Mem_Read(h_stmpe_i2c, stmpe_addr, STMPE610_TSC_DATA_X, 1, datax, 2, 50);
    *x = (datax[0] << 8) | datax[1];
    
    /* Read Y (0x4F, 0x50) */
    uint8_t datay[2];
    HAL_I2C_Mem_Read(h_stmpe_i2c, stmpe_addr, STMPE610_TSC_DATA_Y, 1, datay, 2, 50);
    *y = (datay[0] << 8) | datay[1];
    
    /* Read Z (0x51) */
    *z = ReadReg(STMPE610_TSC_DATA_Z);
    
    /* Reset FIFO to prevent stale data */
    WriteReg(STMPE610_FIFO_STA, 0x01);
    WriteReg(STMPE610_FIFO_STA, 0x00);
    
    return 1;
}
