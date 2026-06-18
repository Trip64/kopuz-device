#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EPD_WIDTH 122
#define EPD_HEIGHT 250
#define EPD_ROW_BYTES 16
#define EPD_FRAME_BYTES (EPD_ROW_BYTES * EPD_HEIGHT)

int epd_init(void);

void epd_clear(void);

void epd_display_frame(const uint8_t *buf);

void epd_display_frame_partial(const uint8_t *buf);

void epd_sleep(void);

#ifdef __cplusplus
}
#endif
