#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "decoder.h"
#include "hal/hal_storage.h"

#define MP3_STREAM_BUF_SIZE 4096

typedef struct {
    hal_file_t *file;
    mp3dec_t mp3d;
    uint8_t stream_buf[MP3_STREAM_BUF_SIZE * 2];
    size_t stream_buf_len;
    size_t stream_buf_pos;
    mp3d_sample_t pcm_frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t pcm_avail;
    size_t pcm_pos;
    uint8_t *cover_data;
    size_t cover_size;
    bool eof;
} mp3_state_t;

// Extract ID3v2 APIC (Cover Art) if present
static void extract_id3_cover(hal_file_t *f, mp3_state_t *st) {
    uint8_t header[10];
    if (hal_fseek(f, 0, 0) != 0) return;
    if (hal_fread(header, 1, 10, f) != 10) return;

    if (memcmp(header, "ID3", 3) != 0) {
        hal_fseek(f, 0, 0);
        return;
    }

    uint8_t ver = header[3];
    uint32_t tag_size = ((uint32_t)(header[6] & 0x7F) << 21) |
                        ((uint32_t)(header[7] & 0x7F) << 14) |
                        ((uint32_t)(header[8] & 0x7F) << 7)  |
                        ((uint32_t)(header[9] & 0x7F));

    if (tag_size == 0 || tag_size > 4 * 1024 * 1024) {
        hal_fseek(f, 10 + tag_size, 0);
        return;
    }

    // Read ID3 tag payload into buffer to look for APIC
    uint8_t *tag_buf = (uint8_t*)malloc(tag_size);
    if (!tag_buf) {
        hal_fseek(f, 10 + tag_size, 0);
        return;
    }

    if (hal_fread(tag_buf, 1, tag_size, f) == tag_size) {
        size_t offset = 0;
        while (offset + 10 < tag_size) {
            const char *id = (const char*)&tag_buf[offset];
            if (id[0] == 0) break; // Padding reached

            uint32_t frame_sz;
            if (ver == 4) {
                frame_sz = ((uint32_t)(tag_buf[offset + 4] & 0x7F) << 21) |
                           ((uint32_t)(tag_buf[offset + 5] & 0x7F) << 14) |
                           ((uint32_t)(tag_buf[offset + 6] & 0x7F) << 7)  |
                           ((uint32_t)(tag_buf[offset + 7] & 0x7F));
            } else {
                frame_sz = ((uint32_t)tag_buf[offset + 4] << 24) |
                           ((uint32_t)tag_buf[offset + 5] << 16) |
                           ((uint32_t)tag_buf[offset + 6] << 8)  |
                           ((uint32_t)tag_buf[offset + 7]);
            }

            if (offset + 10 + frame_sz > tag_size) break;

            if (memcmp(id, "APIC", 4) == 0 && frame_sz > 12) {
                const uint8_t *frame_data = &tag_buf[offset + 10];
                size_t p = 1; // skip encoding byte
                while (p < frame_sz && frame_data[p] != 0) p++; // skip MIME
                p++; // skip MIME null
                if (p < frame_sz) p++; // skip pic type
                while (p < frame_sz && frame_data[p] != 0) p++; // skip description
                p++; // skip description null

                if (p < frame_sz) {
                    size_t img_len = frame_sz - p;
                    // Cap cover at 512KB
                    if (img_len > 0 && img_len <= 512 * 1024) {
                        st->cover_data = (uint8_t*)malloc(img_len);
                        if (st->cover_data) {
                            memcpy(st->cover_data, &frame_data[p], img_len);
                            st->cover_size = img_len;
                        }
                    }
                }
                break;
            }
            offset += 10 + frame_sz;
        }
    }
    free(tag_buf);
    hal_fseek(f, 10 + tag_size, 0);
}

static int mp3_decode(decoder_t *dec, int32_t *out, size_t max_samples) {
    mp3_state_t *st = (mp3_state_t*)dec->user_data;
    if (!st || !st->file) return 0;

    size_t samples_written = 0;

    while (samples_written < max_samples) {
        // Drain any already-decoded PCM from previous frame
        if (st->pcm_pos < st->pcm_avail) {
            size_t n = st->pcm_avail - st->pcm_pos;
            if (n > max_samples - samples_written) {
                n = max_samples - samples_written;
            }
            for (size_t i = 0; i < n; i++) {
                int16_t s16 = st->pcm_frame[st->pcm_pos + i];
                out[samples_written + i] = (int32_t)((uint32_t)s16 << 16);
            }
            st->pcm_pos += n;
            samples_written += n;
            continue;
        }

        // Need to decode next MP3 frame
        // Refill stream buffer if low
        if (st->stream_buf_pos > 0 && st->stream_buf_len > st->stream_buf_pos) {
            memmove(st->stream_buf, &st->stream_buf[st->stream_buf_pos], st->stream_buf_len - st->stream_buf_pos);
            st->stream_buf_len -= st->stream_buf_pos;
            st->stream_buf_pos = 0;
        } else if (st->stream_buf_pos >= st->stream_buf_len) {
            st->stream_buf_len = 0;
            st->stream_buf_pos = 0;
        }

        size_t space = sizeof(st->stream_buf) - st->stream_buf_len;
        if (space > 0 && !st->eof) {
            size_t read_bytes = hal_fread(&st->stream_buf[st->stream_buf_len], 1, space, st->file);
            if (read_bytes > 0) {
                st->stream_buf_len += read_bytes;
            } else {
                st->eof = true;
            }
        }

        size_t available_bytes = st->stream_buf_len - st->stream_buf_pos;
        if (available_bytes == 0) {
            break; // EOF
        }

        mp3dec_frame_info_t info;
        int frame_samples = mp3dec_decode_frame(
            &st->mp3d,
            &st->stream_buf[st->stream_buf_pos],
            (int)available_bytes,
            st->pcm_frame,
            &info
        );

        if (info.frame_bytes > 0) {
            st->stream_buf_pos += (size_t)info.frame_bytes;
        } else {
            // Skip 1 byte if can't sync frame
            st->stream_buf_pos++;
        }

        if (frame_samples > 0) {
            st->pcm_avail = (size_t)frame_samples * (size_t)info.channels;
            st->pcm_pos = 0;
            dec->info.sample_rate = (uint32_t)info.hz;
            dec->info.channels = (uint8_t)info.channels;
        }
    }

    return (int)samples_written;
}

static bool mp3_get_cover(decoder_t *dec, uint8_t **out_data, size_t *out_size) {
    mp3_state_t *st = (mp3_state_t*)dec->user_data;
    if (!st || !st->cover_data || st->cover_size == 0) return false;
    *out_data = st->cover_data;
    *out_size = st->cover_size;
    st->cover_data = NULL; // ownership transferred
    st->cover_size = 0;
    return true;
}

static bool mp3_seek(decoder_t *dec, uint32_t target_sec) {
    mp3_state_t *st = (mp3_state_t*)dec->user_data;
    if (!st || !st->file) return false;
    size_t total_sz = hal_fsize(st->file);
    if (total_sz == 0 || dec->info.duration_secs == 0) return false;

    // Estimate file position based on target seconds and total duration
    uint64_t target_offset = ((uint64_t)target_sec * total_sz) / dec->info.duration_secs;
    if (target_offset >= total_sz) target_offset = total_sz > 1024 ? (total_sz - 1024) : 0;

    hal_fseek(st->file, (long)target_offset, 0);
    st->stream_buf_len = 0;
    st->stream_buf_pos = 0;
    st->pcm_pos = 0;
    st->pcm_avail = 0;
    st->eof = false;
    mp3dec_init(&st->mp3d);
    return true;
}

static void mp3_close(decoder_t *dec) {
    if (!dec) return;
    mp3_state_t *st = (mp3_state_t*)dec->user_data;
    if (st) {
        if (st->file) hal_fclose(st->file);
        if (st->cover_data) free(st->cover_data);
        free(st);
    }
    free(dec);
}

decoder_t* mp3_decoder_open(const char *path) {
    hal_file_t *f = hal_fopen(path, "rb");
    if (!f) return NULL;

    mp3_state_t *st = (mp3_state_t*)calloc(1, sizeof(mp3_state_t));
    if (!st) {
        hal_fclose(f);
        return NULL;
    }

    st->file = f;
    mp3dec_init(&st->mp3d);

    // Extract embedded cover art
    extract_id3_cover(f, st);

    // Calculate approximate duration based on file size and bitrate
    size_t file_sz = hal_fsize(f);
    uint32_t approx_duration = (file_sz > 128 * 1024) ? (uint32_t)((file_sz * 8) / (192 * 1000)) : 0;

    decoder_t *dec = (decoder_t*)calloc(1, sizeof(decoder_t));
    if (!dec) {
        if (st->cover_data) free(st->cover_data);
        free(st);
        hal_fclose(f);
        return NULL;
    }

    dec->info.sample_rate = 44100;
    dec->info.channels = 2;
    dec->info.bits_per_sample = 16;
    dec->info.duration_secs = approx_duration;
    dec->decode = mp3_decode;
    dec->get_cover = mp3_get_cover;
    dec->seek = mp3_seek;
    dec->close = mp3_close;
    dec->user_data = st;

    return dec;
}
