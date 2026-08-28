#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(TARGET_OLED_I2C) || defined(DISPLAY_OLED_I2C) || defined(DISPLAY_SSD1306)
    #define LCD_WIDTH       128
    #define LCD_HEIGHT      64
    #define COLOR_DISPLAY   0
    #define ART_BOX_PX      0
#elif defined(TARGET_EPD) || defined(DISPLAY_EINK_EPD)
    #define LCD_WIDTH       250
    #define LCD_HEIGHT      122
    #define COLOR_DISPLAY   0
    #define ART_BOX_PX      48
#elif defined(TARGET_SHARP_MIP) || defined(DISPLAY_SHARP_MIP)
    #define LCD_WIDTH       400
    #define LCD_HEIGHT      240
    #define COLOR_DISPLAY   0
    #define ART_BOX_PX      48
#elif defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350) || defined(TARGET_QVGA)
    #define LCD_WIDTH       320
    #define LCD_HEIGHT      240
    #define COLOR_DISPLAY   1
    #define ART_BOX_PX      80
#else
    #define LCD_WIDTH       320
    #define LCD_HEIGHT      170
    #define COLOR_DISPLAY   1
    #define ART_BOX_PX      56
#endif

#define LCD_ROW_BYTES_1BPP  ((LCD_WIDTH + 7) / 8)
#define LCD_FRAME_BYTES_1BPP (LCD_ROW_BYTES_1BPP * LCD_HEIGHT)

// Audio Buffer Configuration
#define AUDIO_DEFAULT_SAMPLE_RATE 44100
#define AUDIO_CHANNELS            2
#define AUDIO_BLOCK_FRAMES        1024
#define AUDIO_BUFFER_SAMPLES      (AUDIO_BLOCK_FRAMES * AUDIO_CHANNELS)

// Application Limits
#define MAX_TRACKS                5000
#define MAX_GROUPS                512
#define MAX_PATH_LEN              256
#define MAX_TITLE_LEN             64
#define MAX_NAME_LEN              48

// Storage Mount Point
#define STORAGE_MOUNT_POINT       "/sdcard"

#ifdef __cplusplus
}
#endif
