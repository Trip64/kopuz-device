#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t width;
    uint8_t height;
    char first_char;
    char last_char;
    const uint8_t *data; // 1 bit per pixel or 1 byte per row depending on width
} font_t;

extern const font_t font_6x10;
extern const font_t font_8x13_bold;

#ifdef __cplusplus
}
#endif
