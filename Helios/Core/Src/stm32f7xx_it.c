/**
 ******************************************************************************
 * @file    stm32f7xx_it.c
 * @brief   Interrupt Service Routines for STM32F7xx
 ******************************************************************************
 */
#include "main.h"
#include "stm32f7xx_it.h"

extern UART_HandleTypeDef huart6;

/* Cortex-M7 Processor Interruption and Exception Handlers */

void NMI_Handler(void) { while (1) {} }
void HardFault_Handler(void) { while (1) {} }
void MemManage_Handler(void) { while (1) {} }
void BusFault_Handler(void) { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }

void SysTick_Handler(void) {
    HAL_IncTick();
}

void USART6_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart6);
}
