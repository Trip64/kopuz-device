#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#include "dr_flac.h"
#include "decoder.h"
#include "hal/hal_storage.h"

typedef struct {
    hal_file_t *file;
    drflac *pFlac;
    uint8_t *cover_data;
    size_t cover_size;
} flac_state_t;

static size_t flac_on_read(void *pUserData, void *pBufferOut, size_t bytesToRead) {
    flac_state_t *st = (flac_state_t*)pUserData;
    if (!st || !st->file) return 0;
    return hal_fread(pBufferOut, 1, bytesToRead, st->file);
}

static drflac_bool32 flac_on_seek(void *pUserData, int offset, drflac_seek_origin origin) {
    flac_state_t *st = (flac_state_t*)pUserData;
    if (!st || !st->file) return DRFLAC_FALSE;
    int whence = 0;
    if (origin == DRFLAC_SEEK_CUR) whence = 1;
    else if (origin == DRFLAC_SEEK_END) whence = 2;
    return (hal_fseek(st->file, (long)offset, whence) == 0) ? DRFLAC_TRUE : DRFLAC_FALSE;
}

static drflac_bool32 flac_on_tell(void *pUserData, drflac_int64 *pCursor) {
    flac_state_t *st = (flac_state_t*)pUserData;
    if (!st || !st->file || !pCursor) return DRFLAC_FALSE;
    long pos = hal_ftell(st->file);
    if (pos < 0) return DRFLAC_FALSE;
    *pCursor = (drflac_int64)pos;
    return DRFLAC_TRUE;
}

static void flac_on_meta(void *pUserData, drflac_metadata *pMetadata) {
    flac_state_t *st = (flac_state_t*)pUserData;
    if (!st || !pMetadata) return;

    if (pMetadata->type == DRFLAC_METADATA_BLOCK_TYPE_PICTURE) {
        drflac_uint32 picSize = pMetadata->data.picture.pictureDataSize;
        const void *picData = pMetadata->data.picture.pPictureData;
        if (picSize > 0 && picSize <= 512 * 1024 && picData) {
            st->cover_data = (uint8_t*)malloc(picSize);
            if (st->cover_data) {
                memcpy(st->cover_data, picData, picSize);
                st->cover_size = picSize;
            }
        }
    }
}

static int flac_decode(decoder_t *dec, int32_t *out, size_t max_samples) {
    flac_state_t *st = (flac_state_t*)dec->user_data;
    if (!st || !st->pFlac) return 0;

    uint8_t ch = dec->info.channels ? dec->info.channels : 1;
    drflac_uint64 frames_to_read = max_samples / ch;
    if (frames_to_read == 0) return 0;

    drflac_uint64 frames_read = drflac_read_pcm_frames_s32(st->pFlac, frames_to_read, (drflac_int32*)out);
    if (frames_read == 0) return 0;

    size_t samples = (size_t)frames_read * ch;
    // drflac_read_pcm_frames_s32 already returns full 32-bit scaled PCM samples
    return (int)samples;
}

static bool flac_get_cover(decoder_t *dec, uint8_t **out_data, size_t *out_size) {
    flac_state_t *st = (flac_state_t*)dec->user_data;
    if (!st || !st->cover_data || st->cover_size == 0) return false;
    *out_data = st->cover_data;
    *out_size = st->cover_size;
    st->cover_data = NULL; // transferred
    st->cover_size = 0;
    return true;
}

static bool flac_seek(decoder_t *dec, uint32_t target_sec) {
    flac_state_t *st = (flac_state_t*)dec->user_data;
    if (!st || !st->pFlac) return false;
    drflac_uint64 target_frame = (drflac_uint64)target_sec * dec->info.sample_rate;
    if (st->pFlac->totalPCMFrameCount > 0 && target_frame >= st->pFlac->totalPCMFrameCount) {
        target_frame = st->pFlac->totalPCMFrameCount - 1;
    }
    return (drflac_seek_to_pcm_frame(st->pFlac, target_frame) == DRFLAC_TRUE);
}

static void flac_close(decoder_t *dec) {
    if (!dec) return;
    flac_state_t *st = (flac_state_t*)dec->user_data;
    if (st) {
        if (st->pFlac) drflac_close(st->pFlac);
        if (st->file) hal_fclose(st->file);
        if (st->cover_data) free(st->cover_data);
        free(st);
    }
    free(dec);
}

decoder_t* flac_decoder_open(const char *path) {
    hal_file_t *f = hal_fopen(path, "rb");
    if (!f) return NULL;

    flac_state_t *st = (flac_state_t*)calloc(1, sizeof(flac_state_t));
    if (!st) {
        hal_fclose(f);
        return NULL;
    }
    st->file = f;

    drflac *pFlac = drflac_open_with_metadata(&flac_on_read, &flac_on_seek, &flac_on_tell, &flac_on_meta, st, NULL);
    if (!pFlac) {
        free(st);
        hal_fclose(f);
        return NULL;
    }
    st->pFlac = pFlac;

    uint32_t sample_rate = pFlac->sampleRate;
    uint8_t channels = (uint8_t)pFlac->channels;
    uint8_t bits_per_sample = (uint8_t)pFlac->bitsPerSample;
    uint32_t duration_secs = (sample_rate > 0) ? (uint32_t)(pFlac->totalPCMFrameCount / sample_rate) : 0;

    decoder_t *dec = (decoder_t*)calloc(1, sizeof(decoder_t));
    if (!dec) {
        drflac_close(pFlac);
        free(st);
        hal_fclose(f);
        return NULL;
    }

    dec->info.sample_rate = sample_rate;
    dec->info.channels = channels;
    dec->info.bits_per_sample = bits_per_sample;
    dec->info.duration_secs = duration_secs;
    dec->decode = flac_decode;
    dec->get_cover = flac_get_cover;
    dec->seek = flac_seek;
    dec->close = flac_close;
    dec->user_data = st;

    return dec;
}
