/**
 * @file    : dashboard.h
 * @brief   : SOLARIS Dashboard Interface
 */
#ifndef __DASHBOARD_H
#define __DASHBOARD_H

#include "stm32f7xx_hal.h"

void Dashboard_Init(I2C_HandleTypeDef *hi2c);
void Dashboard_Run(void);

#endif /* __DASHBOARD_H */
