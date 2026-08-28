/**
 ******************************************************************************
 * @file           : diskio_sd.c
 * @brief          : FatFs Disk I/O Driver for SD Card
 ******************************************************************************
 */
#include "diskio.h"
#include "sdcard.h"

/**
 * @brief Get disk status
 */
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    
    if (!SD_IsPresent()) {
        return STA_NODISK;
    }
    
    return 0;
}

/**
 * @brief Initialize disk
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    
    if (SD_Init() != SD_OK) {
        return STA_NOINIT;
    }
    
    return 0;
}

/**
 * @brief Read sectors
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0) return RES_PARERR;
    
    if (SD_ReadBlocks(buff, sector, count) != SD_OK) {
        return RES_ERROR;
    }
    
    return RES_OK;
}

/**
 * @brief Write sectors (not used - read-only config)
 */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    (void)pdrv;
    (void)buff;
    (void)sector;
    (void)count;
    return RES_WRPRT;  /* Write protected */
}

/**
 * @brief Disk I/O control
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;
    
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT:
            *(DWORD*)buff = 0;  /* Not used */
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

/**
 * @brief Get time for FAT (not used with FF_FS_NORTC)
 */
DWORD get_fattime(void)
{
    return ((2024 - 1980) << 25) | (1 << 21) | (1 << 16);
}
