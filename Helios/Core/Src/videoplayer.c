/**
 ******************************************************************************
 * @file           : videoplayer.c
 * @brief          : Bad Apple Video Player Implementation
 * @note           : Plays RLE-compressed 1-bit video from SD card
 ******************************************************************************
 */
#include "videoplayer.h"
#include "ssd1963.h"
#include "main.h"
#include "ff.h"
#include <string.h>

/* File and video state */
static FIL videoFile;
static FATFS fatFs;
static VideoHeader header;
static uint32_t currentFrame = 0;
static uint8_t isPlaying = 0;

/* Frame buffer for RLE data (max ~8KB per frame) */
static uint8_t rleBuffer[16384];

/* Offset table for seeking */
static uint32_t *frameOffsets = NULL;

/* Colors for B&W display */
#define COLOR_BG    COLOR_BLACK
#define COLOR_FG    COLOR_WHITE

/**
 * @brief Decode RLE and render frame directly to display
 */
static void RenderFrame(uint8_t *rleData, uint32_t rleSize)
{
    uint16_t x = 0, y = 0;
    uint32_t rlePos = 0;
    
    /* Set window for entire display */
    SSD1963_SetWindow(0, 0, header.width - 1, header.height - 1);
    
    /* Start writing pixels */
    TFT_CS_PORT->BSRR = (uint32_t)TFT_CS_PIN << 16;  /* CS low */
    TFT_RS_PORT->BSRR = TFT_RS_PIN;  /* RS high (data) */
    
    while (rlePos < rleSize && y < header.height) {
        uint8_t count = rleData[rlePos++];
        uint8_t value = rleData[rlePos++];
        
        /* Each byte contains 8 pixels */
        for (uint8_t byte = 0; byte < count && y < header.height; byte++) {
            /* Expand 8 bits to 8 pixels */
            for (int8_t bit = 7; bit >= 0 && y < header.height; bit--) {
                uint16_t color = (value & (1 << bit)) ? COLOR_FG : COLOR_BG;
                
                /* Write 16-bit color to split data bus */
                uint32_t temp;
                temp = TFT_DATA_HI_PORT->ODR;
                temp &= ~TFT_DATA_HI_MASK;
                TFT_DATA_HI_PORT->ODR = temp | (color & 0xFF00);
                temp = TFT_DATA_LO_PORT->ODR;
                temp &= ~TFT_DATA_LO_MASK;
                TFT_DATA_LO_PORT->ODR = temp | (color & 0x00FF);
                
                /* Write strobe */
                TFT_WR_PORT->BSRR = (uint32_t)TFT_WR_PIN << 16;  /* WR low */
                __NOP();
                TFT_WR_PORT->BSRR = TFT_WR_PIN;  /* WR high */
                
                x++;
                if (x >= header.width) {
                    x = 0;
                    y++;
                }
            }
        }
    }
    
    TFT_CS_PORT->BSRR = TFT_CS_PIN;  /* CS high */
}

/**
 * @brief Initialize video player
 */
VideoStatus Video_Init(const char *filename)
{
    FRESULT res;
    UINT bytesRead;
    
    /* Mount filesystem */
    res = f_mount(&fatFs, "", 1);
    if (res != FR_OK) {
        return VIDEO_ERROR_SD;
    }
    
    /* Open video file */
    res = f_open(&videoFile, filename, FA_READ);
    if (res != FR_OK) {
        return VIDEO_ERROR_FILE;
    }
    
    /* Read header */
    res = f_read(&videoFile, &header, sizeof(header), &bytesRead);
    if (res != FR_OK || bytesRead != sizeof(header)) {
        f_close(&videoFile);
        return VIDEO_ERROR_FORMAT;
    }
    
    /* Verify magic */
    if (memcmp(header.magic, "BAPV", 4) != 0) {
        f_close(&videoFile);
        return VIDEO_ERROR_FORMAT;
    }
    
    /* Verify dimensions match display */
    if (header.width != TFT_WIDTH || header.height != TFT_HEIGHT) {
        /* Allow anyway, just won't fill screen */
    }
    
    currentFrame = 0;
    isPlaying = 0;
    
    return VIDEO_OK;
}

/**
 * @brief Play video (blocking until complete)
 */
VideoStatus Video_Play(void)
{
    FRESULT res;
    UINT bytesRead;
    uint32_t frameSize;
    uint32_t frameTime_ms = 1000 / header.fps;
    uint32_t lastFrameTime = HAL_GetTick();
    
    isPlaying = 1;
    
    /* Skip offset table (we read sequentially) */
    f_lseek(&videoFile, 16 + header.frame_count * 4);
    
    for (currentFrame = 0; currentFrame < header.frame_count && isPlaying; currentFrame++) {
        uint32_t startTime = HAL_GetTick();
        
        /* Read frame size */
        res = f_read(&videoFile, &frameSize, 4, &bytesRead);
        if (res != FR_OK || bytesRead != 4) {
            return VIDEO_ERROR_FILE;
        }
        
        /* Read RLE data */
        if (frameSize > sizeof(rleBuffer)) {
            frameSize = sizeof(rleBuffer);  /* Truncate if too large */
        }
        
        res = f_read(&videoFile, rleBuffer, frameSize, &bytesRead);
        if (res != FR_OK || bytesRead != frameSize) {
            return VIDEO_ERROR_FILE;
        }
        
        /* Render frame */
        RenderFrame(rleBuffer, frameSize);
        
        /* Frame rate control */
        uint32_t elapsed = HAL_GetTick() - startTime;
        if (elapsed < frameTime_ms) {
            HAL_Delay(frameTime_ms - elapsed);
        }
    }
    
    isPlaying = 0;
    f_close(&videoFile);
    
    return VIDEO_OK;
}

/**
 * @brief Stop playback
 */
void Video_Stop(void)
{
    isPlaying = 0;
}

/**
 * @brief Get current frame number
 */
uint32_t Video_GetCurrentFrame(void)
{
    return currentFrame;
}

/**
 * @brief Get total frame count
 */
uint32_t Video_GetTotalFrames(void)
{
    return header.frame_count;
}
