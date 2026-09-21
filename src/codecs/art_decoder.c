#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#include "stb_image.h"
#include "decoder.h"

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

bool decode_art_rgb565(const uint8_t *jpeg_bytes, size_t jpeg_len, uint16_t target_px, uint8_t *out_rgb565) {
    if (!jpeg_bytes || jpeg_len == 0 || jpeg_len > INT_MAX || target_px == 0 || !out_rgb565) {
        return false;
    }

    int w = 0, h = 0, channels = 0;
    if (!stbi_info_from_memory(jpeg_bytes, (int)jpeg_len, &w, &h, &channels)) {
        return false;
    }
    if (w > 2048 || h > 2048 || w <= 0 || h <= 0) {
        return false;
    }

    size_t pixel_count = (size_t)w * (size_t)h;
    if (pixel_count > SIZE_MAX / 3) return false;
#if defined(ESP_PLATFORM)
    size_t decoded_bytes = pixel_count * 3;
    /* stb_image expands the entire JPEG. Refuse artwork that would exhaust or
       badly fragment the non-PSRAM heap; playback is more important than art. */
    size_t free_bytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (decoded_bytes > 64U * 1024U || decoded_bytes > largest_block ||
        decoded_bytes + 48U * 1024U > free_bytes) {
        return false;
    }
#endif

    unsigned char *pixels = stbi_load_from_memory(jpeg_bytes, (int)jpeg_len, &w, &h, &channels, 3);
    if (!pixels) {
        return false;
    }

    // Downscale box-sampling to target_px x target_px
    for (uint16_t ty = 0; ty < target_px; ty++) {
        uint32_t sy0 = (uint32_t)ty * (uint32_t)h / target_px;
        uint32_t sy1 = ((uint32_t)(ty + 1) * (uint32_t)h / target_px);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        if (sy1 > (uint32_t)h) sy1 = (uint32_t)h;

        for (uint16_t tx = 0; tx < target_px; tx++) {
            uint32_t sx0 = (uint32_t)tx * (uint32_t)w / target_px;
            uint32_t sx1 = ((uint32_t)(tx + 1) * (uint32_t)w / target_px);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sx1 > (uint32_t)w) sx1 = (uint32_t)w;

            uint32_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
            for (uint32_t sy = sy0; sy < sy1; sy++) {
                for (uint32_t sx = sx0; sx < sx1; sx++) {
                    size_t idx = ((size_t)sy * (size_t)w + (size_t)sx) * 3;
                    r_sum += pixels[idx];
                    g_sum += pixels[idx + 1];
                    b_sum += pixels[idx + 2];
                    count++;
                }
            }
            if (count == 0) count = 1;
            uint8_t r = (uint8_t)(r_sum / count);
            uint8_t g = (uint8_t)(g_sum / count);
            uint8_t b = (uint8_t)(b_sum / count);

            // RGB565 big-endian
            uint16_t rgb565 = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
            size_t out_idx = ((size_t)ty * target_px + (size_t)tx) * 2;
            out_rgb565[out_idx] = (uint8_t)(rgb565 >> 8);
            out_rgb565[out_idx + 1] = (uint8_t)(rgb565 & 0xFF);
        }
    }

    stbi_image_free(pixels);
    return true;
}
