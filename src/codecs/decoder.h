#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint32_t duration_secs;
} stream_info_t;

typedef struct decoder_s decoder_t;

struct decoder_s {
    stream_info_t info;
    // Decode up to max_samples (interleaved int32_t). Returns number of samples decoded, 0 on EOF, negative on error.
    int (*decode)(decoder_t *dec, int32_t *out, size_t max_samples);
    // Retrieve cover art image data (JPEG) if embedded in track metadata
    bool (*get_cover)(decoder_t *dec, uint8_t **out_data, size_t *out_size);
    // Seek to seconds (optional, returns true if successful)
    bool (*seek)(decoder_t *dec, uint32_t target_sec);
    // Close and free resources
    void (*close)(decoder_t *dec);
    void *user_data;
};

// Open a file path and return the appropriate decoder (WAV, MP3, FLAC)
decoder_t* decoder_open(const char *path);

// Decode embedded album art JPEG and downscale directly to target_px x target_px RGB565 buffer
// Returns true on success, allocating or writing to out_rgb565
bool decode_art_rgb565(const uint8_t *jpeg_bytes, size_t jpeg_len, uint16_t target_px, uint8_t *out_rgb565);

#ifdef __cplusplus
}
#endif
