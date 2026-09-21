#include "codecs/decoder.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

decoder_t *wav_decoder_open(const char *path) {
    (void)path;
    return NULL;
}

static int exercise_file(const char *path, const uint8_t *bytes, size_t length) {
    FILE *file = fopen(path, "wb");
    if (!file) return 1;
    if (length > 0 && fwrite(bytes, 1, length, file) != length) {
        fclose(file);
        return 1;
    }
    fclose(file);

    decoder_t *decoder = decoder_open(path);
    if (decoder) {
        int32_t pcm[256];
        for (int attempt = 0; attempt < 8; ++attempt) {
            int decoded = decoder->decode(decoder, pcm, 256);
            if (decoded < 0 || decoded > 256) {
                decoder->close(decoder);
                remove(path);
                return 1;
            }
            if (decoded == 0) break;
        }
        decoder->close(decoder);
    }
    remove(path);
    return 0;
}

static int decode_existing(const char *path) {
    decoder_t *decoder = decoder_open(path);
    if (!decoder) return 1;
    int32_t pcm[256];
    size_t total = 0;
    for (unsigned attempt = 0; attempt < 100000; ++attempt) {
        int decoded = decoder->decode(decoder, pcm, 256);
        if (decoded < 0 || decoded > 256) {
            decoder->close(decoder);
            return 1;
        }
        if (decoded == 0) break;
        total += (size_t)decoded;
    }
    decoder->close(decoder);
    return total > 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (decode_existing(argv[i]) != 0) return 1;
        }
        puts("MP3/FLAC decode smoke test passed");
        return 0;
    }

    uint8_t data[8192];
    uint32_t state = 0x4b6f7075U;

    for (unsigned iteration = 0; iteration < 64; ++iteration) {
        size_t length = (iteration * 127U) % sizeof(data);
        for (size_t i = 0; i < length; ++i) {
            state = state * 1664525U + 1013904223U;
            data[i] = (uint8_t)(state >> 24);
        }
        if (exercise_file("kopuz_bad.mp3", data, length) != 0) return 1;
        if (exercise_file("kopuz_bad.flac", data, length) != 0) return 1;
    }

    /* Oversized/truncated ID3 metadata must fail without allocating its claim. */
    const uint8_t truncated_id3[] = {
        'I', 'D', '3', 4, 0, 0, 0x7f, 0x7f, 0x7f, 0x7f,
        'A', 'P', 'I', 'C', 0, 0, 0, 32, 0, 0
    };
    if (exercise_file("kopuz_bad.mp3", truncated_id3, sizeof(truncated_id3)) != 0) return 1;

    puts("Malformed MP3/FLAC decoder tests passed");
    return 0;
}
