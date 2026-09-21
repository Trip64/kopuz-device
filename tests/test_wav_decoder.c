#include "codecs/decoder.h"
#include "hal/hal_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

decoder_t *mp3_decoder_open(const char *path) { (void)path; return NULL; }
decoder_t *flac_decoder_open(const char *path) { (void)path; return NULL; }

static void write_le16(FILE *file, uint16_t value) {
    fputc((int)(value & 0xFFu), file);
    fputc((int)((value >> 8) & 0xFFu), file);
}

static void write_le32(FILE *file, uint32_t value) {
    write_le16(file, (uint16_t)(value & 0xFFFFu));
    write_le16(file, (uint16_t)(value >> 16));
}

static void create_test_wav(const char *path) {
    const uint32_t sample_rate = 8000;
    const uint32_t sample_count = sample_rate * 2;
    const uint32_t data_size = sample_count * 2;
    const uint32_t junk_size = 5;
    const uint32_t riff_size = 4 + (8 + junk_size + 1) + (8 + 16) + (8 + data_size);

    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    fwrite("RIFF", 1, 4, file);
    write_le32(file, riff_size);
    fwrite("WAVE", 1, 4, file);
    fwrite("JUNK", 1, 4, file);
    write_le32(file, junk_size);
    fwrite("abcde", 1, junk_size, file);
    fputc(0, file);
    fwrite("fmt ", 1, 4, file);
    write_le32(file, 16);
    write_le16(file, 1);
    write_le16(file, 1);
    write_le32(file, sample_rate);
    write_le32(file, sample_rate * 2);
    write_le16(file, 2);
    write_le16(file, 16);
    fwrite("data", 1, 4, file);
    write_le32(file, data_size);
    for (uint32_t i = 0; i < sample_count; i++) {
        write_le16(file, (uint16_t)(i < sample_rate ? (int16_t)-1000 : (int16_t)1000));
    }
    CHECK(fclose(file) == 0);
}

hal_file_t *hal_fopen(const char *path, const char *mode) { return (hal_file_t*)fopen(path, mode); }
size_t hal_fread(void *ptr, size_t size, size_t count, hal_file_t *file) { return fread(ptr, size, count, (FILE*)file); }
size_t hal_fwrite(const void *ptr, size_t size, size_t count, hal_file_t *file) { return fwrite(ptr, size, count, (FILE*)file); }
int hal_fseek(hal_file_t *file, long offset, int whence) { return fseek((FILE*)file, offset, whence); }
long hal_ftell(hal_file_t *file) { return ftell((FILE*)file); }
int hal_fclose(hal_file_t *file) { return fclose((FILE*)file); }
size_t hal_fsize(hal_file_t *file) {
    long current = ftell((FILE*)file);
    CHECK(current >= 0);
    CHECK(fseek((FILE*)file, 0, SEEK_END) == 0);
    long end = ftell((FILE*)file);
    CHECK(fseek((FILE*)file, current, SEEK_SET) == 0);
    return end > 0 ? (size_t)end : 0;
}

int main(void) {
    const char *path = "kopuz_decoder_test.wav";
    create_test_wav(path);

    decoder_t *decoder = decoder_open(path);
    CHECK(decoder != NULL);
    CHECK(decoder->info.sample_rate == 8000);
    CHECK(decoder->info.channels == 1);
    CHECK(decoder->info.bits_per_sample == 16);
    CHECK(decoder->info.duration_secs == 2);

    int32_t samples[16] = {0};
    CHECK(decoder->decode(decoder, samples, 16) == 16);
    CHECK(samples[0] == -1000 * 65536);
    CHECK(decoder->seek(decoder, 1));
    CHECK(decoder->decode(decoder, samples, 16) == 16);
    CHECK(samples[0] == 1000 * 65536);
    CHECK(decoder->seek(decoder, 99));
    CHECK(decoder->decode(decoder, samples, 16) == 0);

    decoder->close(decoder);
    CHECK(remove(path) == 0);
    puts("WAV decoder tests passed");
    return 0;
}
