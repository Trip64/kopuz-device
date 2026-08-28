/**
 ******************************************************************************
 * @file           : sdcard.c
 * @brief          : SD Card Driver using HAL SDMMC (SDIO 4-bit mode)
 * @note           : Pins from mikromedia Plus examples:
 *                   SDIO_D0-D3: PC8-PC11, CMD: PD2, CLK: PC12
 ******************************************************************************
 */
#include "sdcard.h"

/* SDMMC handle */
SD_HandleTypeDef hsd;

/* Card detect pin */
#define SD_CD_PIN      GPIO_PIN_3
#define SD_CD_PORT     GPIOD

/* Private functions */
static void SD_GPIO_Init(void);

/**
 * @brief Check if SD card is present
 */
uint8_t SD_IsPresent(void)
{
    /* Card detect is active low */
    return (HAL_GPIO_ReadPin(SD_CD_PORT, SD_CD_PIN) == GPIO_PIN_RESET) ? 1 : 0;
}

/**
 * @brief Initialize GPIO for SDIO
 */
static void SD_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable clocks */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SDMMC1_CLK_ENABLE();
    
    /* SDIO data pins: PC8, PC9, PC10, PC11 (D0-D3) 
       SDIO clock: PC12 */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* SDIO command: PD2 */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    
    /* Card detect: PD3 (input with pull-up) */
    GPIO_InitStruct.Pin = SD_CD_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SD_CD_PORT, &GPIO_InitStruct);
}

/**
 * @brief Initialize SD card
 */
SD_Status SD_Init(void)
{
    HAL_StatusTypeDef status;
    
    /* Initialize GPIO */
    SD_GPIO_Init();
    
    /* Small delay for card to stabilize after power-up */
    HAL_Delay(100);
    
    /* Check card presence */
    if (!SD_IsPresent()) {
        return SD_NO_CARD;
    }
    
    /* Configure SDMMC peripheral with slower clock for init */
    hsd.Instance = SDMMC1;
    hsd.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide = SDMMC_BUS_WIDE_1B;  /* Start with 1-bit, widen later */
    hsd.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv = 118;  /* Much slower: 216MHz / (118+2) = 1.8MHz for init */
    
    /* Initialize SD card */
    status = HAL_SD_Init(&hsd);
    if (status != HAL_OK) {
        return SD_ERROR;
    }
    
    /* Enable 4-bit wide bus operation */
    status = HAL_SD_ConfigWideBusOperation(&hsd, SDMMC_BUS_WIDE_4B);
    if (status != HAL_OK) {
        /* Continue with 1-bit if 4-bit fails */
    }
    
    return SD_OK;
}

/**
 * @brief Read blocks from SD card
 */
SD_Status SD_ReadBlocks(uint8_t *buf, uint32_t block, uint32_t count)
{
    HAL_StatusTypeDef status;
    uint32_t timeout = 100000;
    
    status = HAL_SD_ReadBlocks(&hsd, buf, block, count, timeout);
    if (status != HAL_OK) {
        return SD_ERROR;
    }
    
    /* Wait for card to be ready */
    while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
        if (--timeout == 0) {
            return SD_TIMEOUT;
        }
    }
    
    return SD_OK;
}

/**
 * @brief Write blocks to SD card
 */
SD_Status SD_WriteBlocks(uint8_t *buf, uint32_t block, uint32_t count)
{
    HAL_StatusTypeDef status;
    uint32_t timeout = 100000;
    
    status = HAL_SD_WriteBlocks(&hsd, buf, block, count, timeout);
    if (status != HAL_OK) {
        return SD_ERROR;
    }
    
    /* Wait for card to be ready */
    while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {
        if (--timeout == 0) {
            return SD_TIMEOUT;
        }
    }
    
    return SD_OK;
}

/**
 * @brief Get SD card info
 */
SD_Status SD_GetCardInfo(SD_CardInfo *info)
{
    HAL_SD_CardInfoTypeDef cardInfo;
    
    if (HAL_SD_GetCardInfo(&hsd, &cardInfo) != HAL_OK) {
        return SD_ERROR;
    }
    
    info->CardCapacity = cardInfo.BlockNbr * cardInfo.BlockSize;
    info->CardBlockSize = cardInfo.BlockSize;
    info->RCA = cardInfo.RelCardAdd;
    info->CardType = cardInfo.CardType;
    
    return SD_OK;
}
