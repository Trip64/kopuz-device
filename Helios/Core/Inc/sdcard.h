/**
 ******************************************************************************
 * @file           : sdcard.h
 * @brief          : SD Card Driver Header (SDIO 4-bit mode)
 ******************************************************************************
 */
#ifndef __SDCARD_H
#define __SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include <stdint.h>

/* SD Card Status */
typedef enum {
    SD_OK = 0,
    SD_ERROR,
    SD_TIMEOUT,
    SD_NO_CARD,
    SD_NOT_READY
} SD_Status;

/* SD Card Info */
typedef struct {
    uint32_t CardCapacity;
    uint32_t CardBlockSize;
    uint16_t RCA;
    uint8_t  CardType;
} SD_CardInfo;

/* Card Types */
#define SD_STD_CAPACITY_V1_1    0
#define SD_STD_CAPACITY_V2_0    1
#define SD_HIGH_CAPACITY        2

/* Functions */
SD_Status SD_Init(void);
SD_Status SD_ReadBlocks(uint8_t *buf, uint32_t block, uint32_t count);
SD_Status SD_WriteBlocks(uint8_t *buf, uint32_t block, uint32_t count);
uint8_t   SD_IsPresent(void);
SD_Status SD_GetCardInfo(SD_CardInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* __SDCARD_H */
