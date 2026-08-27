#include "decoder.h"
#include "hal/hal_storage.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    hal_file_t *file;
    uint32_t data_bytes_left;
    uint32_t data_bytes_total;
    uint16_t bytes_per_sample;
} wav_state_t;

static int wav_decode(decoder_t *dec, int32_t *out, size_t max_samples) {
    wav_state_t *st = (wav_state_t*)dec->user_data;
    if (!st || !st->file || st->data_bytes_left == 0) return 0;

    size_t samples_to_read = max_samples;
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
                int16_t s16 = (int16_t)(buffer[i * 2] | (buffer[i * 2 + 1] << 8));
                out[samples_decoded + i] = (int32_t)((uint32_t)s16 << 16);
            } else if (st->bytes_per_sample == 3) {
                int32_t s24 = (int32_t)((buffer[i * 3] << 8) | (buffer[i * 3 + 1] << 16) | (buffer[i * 3 + 2] << 24));
                out[samples_decoded + i] = s24;
            } else if (st->bytes_per_sample == 4) {
                int32_t s32 = (int32_t)(buffer[i * 4] | (buffer[i * 4 + 1] << 8) | (buffer[i * 4 + 2] << 16) | (buffer[i * 4 + 3] << 24));
                out[samples_decoded + i] = s32;
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
    uint32_t byte_rate = dec->info.sample_rate * dec->info.channels * st->bytes_per_sample;
    uint32_t byte_offset = 44 + target_sec * byte_rate;
    if (st->data_bytes_total > 0 && byte_offset >= 44 + st->data_bytes_total) {
        byte_offset = 44 + st->data_bytes_total;
    }
    st->data_bytes_left = (st->data_bytes_total > 0 && byte_offset < 44 + st->data_bytes_total) ?
                          ((44 + st->data_bytes_total) - byte_offset) : 0;
    return (hal_fseek(st->file, (long)byte_offset, 0) == 0);
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

    uint8_t hdr[44];
    if (hal_fread(hdr, 1, 44, f) != 44) {
        hal_fclose(f);
        return NULL;
    }

    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(&hdr[8], "WAVE", 4) != 0) {
        hal_fclose(f);
        return NULL;
    }

    uint16_t channels = hdr[22] | (hdr[23] << 8);
    uint32_t sample_rate = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | (hdr[27] << 24);
    uint16_t bits_per_sample = hdr[34] | (hdr[35] << 8);
    uint32_t data_len = hdr[40] | (hdr[41] << 8) | (hdr[42] << 16) | (hdr[43] << 24);

    if (channels == 0 || sample_rate == 0 || bits_per_sample == 0) {
        hal_fclose(f);
        return NULL;
    }

    uint32_t bytes_per_sec = sample_rate * channels * (bits_per_sample / 8);
    uint32_t duration_secs = bytes_per_sec > 0 ? (data_len / bytes_per_sec) : 0;

    wav_state_t *st = (wav_state_t*)calloc(1, sizeof(wav_state_t));
    if (!st) {
        hal_fclose(f);
        return NULL;
    }

    st->file = f;
    st->data_bytes_total = data_len;
    st->data_bytes_left = data_len;
    st->bytes_per_sample = bits_per_sample / 8;

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
