#include "decoder.h"
#include "hal/hal_storage.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    hal_file_t *file;
    uint32_t data_bytes_left;
    uint32_t data_bytes_total;
    uint16_t bytes_per_sample;
    uint16_t block_align;
    long data_offset;
} wav_state_t;

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int wav_decode(decoder_t *dec, int32_t *out, size_t max_samples) {
    wav_state_t *st = (wav_state_t*)dec->user_data;
    if (!st || !st->file || st->data_bytes_left == 0) return 0;

    size_t samples_to_read = max_samples;
    if (samples_to_read > SIZE_MAX / st->bytes_per_sample) {
        samples_to_read = SIZE_MAX / st->bytes_per_sample;
    }
    size_t bytes_needed = samples_to_read * st->bytes_per_sample;
    if (bytes_needed > st->data_bytes_left) {
        bytes_needed = st->data_bytes_left;
        samples_to_read = bytes_needed / st->bytes_per_sample;
    }
    if (samples_to_read == 0) return 0;

    uint8_t buffer[2048];
    size_t samples_decoded = 0;

    while (samples_decoded < samples_to_read) {
        size_t chunk_samples = samples_to_read - samples_decoded;
        size_t chunk_bytes = chunk_samples * st->bytes_per_sample;
        if (chunk_bytes > sizeof(buffer)) {
            chunk_bytes = sizeof(buffer);
            chunk_samples = chunk_bytes / st->bytes_per_sample;
        }

        size_t read_bytes = hal_fread(buffer, 1, chunk_bytes, st->file);
        if (read_bytes == 0) break;

        size_t actual_samples = read_bytes / st->bytes_per_sample;
        for (size_t i = 0; i < actual_samples; i++) {
            if (st->bytes_per_sample == 2) {
                int16_t s16 = (int16_t)read_le16(&buffer[i * 2]);
                out[samples_decoded + i] = (int32_t)s16 * 65536;
            } else if (st->bytes_per_sample == 3) {
                uint32_t packed = (uint32_t)buffer[i * 3] |
                                  ((uint32_t)buffer[i * 3 + 1] << 8) |
                                  ((uint32_t)buffer[i * 3 + 2] << 16);
                int32_t s24 = (packed & 0x00800000u) ?
                              (int32_t)(packed | 0xFF000000u) : (int32_t)packed;
                s24 *= 256;
                out[samples_decoded + i] = s24;
            } else if (st->bytes_per_sample == 4) {
                out[samples_decoded + i] = (int32_t)read_le32(&buffer[i * 4]);
            }
        }

        samples_decoded += actual_samples;
        st->data_bytes_left -= (uint32_t)read_bytes;
        if (read_bytes < chunk_bytes) break;
    }

    return (int)samples_decoded;
}

static bool wav_get_cover(decoder_t *dec, uint8_t **out_data, size_t *out_size) {
    (void)dec; (void)out_data; (void)out_size;
    return false; // WAV rarely has embedded pictures
}

static bool wav_seek(decoder_t *dec, uint32_t target_sec) {
    wav_state_t *st = (wav_state_t*)dec->user_data;
    if (!st || !st->file) return false;
    uint64_t byte_rate = (uint64_t)dec->info.sample_rate * st->block_align;
    uint64_t relative_offset = (uint64_t)target_sec * byte_rate;
    if (relative_offset > st->data_bytes_total) relative_offset = st->data_bytes_total;
    relative_offset -= relative_offset % st->block_align;

    uint64_t absolute_offset = (uint64_t)st->data_offset + relative_offset;
    if (absolute_offset > LONG_MAX) return false;
    if (hal_fseek(st->file, (long)absolute_offset, 0) != 0) return false;

    st->data_bytes_left = st->data_bytes_total - (uint32_t)relative_offset;
    return true;
}

static void wav_close(decoder_t *dec) {
    if (!dec) return;
    wav_state_t *st = (wav_state_t*)dec->user_data;
    if (st) {
        if (st->file) hal_fclose(st->file);
        free(st);
    }
    free(dec);
}

decoder_t* wav_decoder_open(const char *path) {
    hal_file_t *f = hal_fopen(path, "rb");
    if (!f) return NULL;

    uint8_t riff_header[12];
    if (hal_fread(riff_header, 1, sizeof(riff_header), f) != sizeof(riff_header)) {
        hal_fclose(f);
        return NULL;
    }

    if (memcmp(riff_header, "RIFF", 4) != 0 || memcmp(&riff_header[8], "WAVE", 4) != 0) {
        hal_fclose(f);
        return NULL;
    }

    bool have_format = false;
    bool have_data = false;
    uint16_t audio_format = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t block_align = 0;
    uint16_t bits_per_sample = 0;
    uint32_t data_len = 0;
    long data_offset = 0;

    while (!have_format || !have_data) {
        uint8_t chunk_header[8];
        if (hal_fread(chunk_header, 1, sizeof(chunk_header), f) != sizeof(chunk_header)) break;
        uint32_t chunk_size = read_le32(&chunk_header[4]);
        long chunk_data_offset = hal_ftell(f);
        if (chunk_data_offset < 0 || chunk_size > (uint32_t)LONG_MAX) break;

        if (memcmp(chunk_header, "fmt ", 4) == 0) {
            uint8_t format_data[16];
            if (chunk_size < sizeof(format_data) ||
                hal_fread(format_data, 1, sizeof(format_data), f) != sizeof(format_data)) break;
            audio_format = read_le16(&format_data[0]);
            channels = read_le16(&format_data[2]);
            sample_rate = read_le32(&format_data[4]);
            block_align = read_le16(&format_data[12]);
            bits_per_sample = read_le16(&format_data[14]);
            have_format = true;
        } else if (memcmp(chunk_header, "data", 4) == 0) {
            data_offset = chunk_data_offset;
            data_len = chunk_size;
            have_data = true;
        }

        if (have_format && have_data) break;

        uint64_t next_offset = (uint64_t)chunk_data_offset + chunk_size + (chunk_size & 1u);
        if (next_offset > LONG_MAX || hal_fseek(f, (long)next_offset, 0) != 0) break;
    }

    uint16_t bytes_per_sample = (uint16_t)(bits_per_sample / 8);
    uint32_t expected_align = (uint32_t)channels * bytes_per_sample;
    if (!have_format || !have_data || audio_format != 1 ||
        channels == 0 || channels > 2 || sample_rate < 8000 || sample_rate > 192000 ||
        (bits_per_sample != 16 && bits_per_sample != 24 && bits_per_sample != 32) ||
        block_align == 0 || block_align != expected_align) {
        hal_fclose(f);
        return NULL;
    }

    size_t file_size = hal_fsize(f);
    if (file_size > 0) {
        if ((uint64_t)data_offset > file_size) {
            hal_fclose(f);
            return NULL;
        }
        size_t available = file_size - (size_t)data_offset;
        if (data_len > available) data_len = (uint32_t)available;
    }
    data_len -= data_len % block_align;

    uint64_t bytes_per_sec = (uint64_t)sample_rate * block_align;
    uint32_t duration_secs = bytes_per_sec > 0 ? (uint32_t)(data_len / bytes_per_sec) : 0;

    wav_state_t *st = (wav_state_t*)calloc(1, sizeof(wav_state_t));
    if (!st) {
        hal_fclose(f);
        return NULL;
    }

    st->file = f;
    st->data_bytes_total = data_len;
    st->data_bytes_left = data_len;
    st->bytes_per_sample = bytes_per_sample;
    st->block_align = block_align;
    st->data_offset = data_offset;

    if (hal_fseek(f, data_offset, 0) != 0) {
        free(st);
        hal_fclose(f);
        return NULL;
    }

    decoder_t *dec = (decoder_t*)calloc(1, sizeof(decoder_t));
    if (!dec) {
        free(st);
        hal_fclose(f);
        return NULL;
    }

    dec->info.sample_rate = sample_rate;
    dec->info.channels = (uint8_t)channels;
    dec->info.bits_per_sample = (uint8_t)bits_per_sample;
    dec->info.duration_secs = duration_secs;
    dec->decode = wav_decode;
    dec->get_cover = wav_get_cover;
    dec->seek = wav_seek;
    dec->close = wav_close;
    dec->user_data = st;

    return dec;
}
