#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LCD_WIDTH       480
#define MAX_LCD_HEIGHT      272
#define MAX_LCD_ROW_BYTES   ((MAX_LCD_WIDTH + 7) / 8)
#define MAX_LCD_FRAME_BYTES (MAX_LCD_ROW_BYTES * MAX_LCD_HEIGHT)

typedef enum {
    DISP_PROFILE_STM32F7_480X272 = 0,
    DISP_PROFILE_ILI9341_320X240,
    DISP_PROFILE_TDISPLAY_320X170,
    DISP_PROFILE_OLED_128X64,
    DISP_PROFILE_OLED_128X128,
    DISP_PROFILE_SHARP_400X240,
    DISP_PROFILE_EPAPER_BWR_296X128,
    DISP_PROFILE_COUNT
} disp_profile_t;

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
#elif defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA) || defined(TARGET_WQVGA)
    #define LCD_WIDTH       480
    #define LCD_HEIGHT      272
    #define COLOR_DISPLAY   1
    #define ART_BOX_PX      80
#elif defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350) || defined(TARGET_QVGA)
    #define LCD_WIDTH       320
    #define LCD_HEIGHT      240
    #define COLOR_DISPLAY   1
    #define ART_BOX_PX      80
#elif defined(TARGET_ESP32S3) || defined(TARGET_TDISPLAY)
    #define LCD_WIDTH       320
    #define LCD_HEIGHT      170
    #define COLOR_DISPLAY   1
    #define ART_BOX_PX      56
#else
    #define LCD_WIDTH       320
    #define LCD_HEIGHT      240
    #define COLOR_DISPLAY   1
    #define ART_BOX_PX      80
#endif

#define LCD_ROW_BYTES_1BPP  ((LCD_WIDTH + 7) / 8)
#define LCD_FRAME_BYTES_1BPP (LCD_ROW_BYTES_1BPP * LCD_HEIGHT)

// Audio Buffer Configuration
#define AUDIO_DEFAULT_SAMPLE_RATE 44100
#define AUDIO_CHANNELS            2
#define AUDIO_BLOCK_FRAMES        1024
#define AUDIO_BUFFER_SAMPLES      (AUDIO_BLOCK_FRAMES * AUDIO_CHANNELS)

// Application Limits
#if defined(KOPUZ_SIMULATOR)
    #define MAX_TRACKS                5000
    #define MAX_GROUPS                512
#else
    #define MAX_TRACKS                128
    #define MAX_GROUPS                64
#endif

#if defined(TARGET_NRF52) || defined(TARGET_NRF54) || defined(TARGET_ESP32) || defined(ESP_PLATFORM) || defined(TARGET_ESP32S3) || defined(TARGET_SIMULATOR)
    #define HAS_BLE_AUDIO             1
#else
    #define HAS_BLE_AUDIO             0
#endif

#define MAX_PATH_LEN              256
#define MAX_TITLE_LEN             64
#define MAX_NAME_LEN              48

// Storage Mount Point
#define STORAGE_MOUNT_POINT       "/sdcard"

#ifdef __cplusplus
}
#endif
