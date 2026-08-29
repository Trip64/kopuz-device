#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

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
    uint8_t raw_channels;
    bool downmix_multichannel;
    bool decimate_2x;
} flac_state_t;

static size_t flac_on_read(void *pUserData, void *pBufferOut, size_t bytesToRead) {
    flac_state_t *st = (flac_state_t*)pUserData;
    if (!st || !st->file) return 0;
    return hal_fread(pBufferOut, 1, bytesToRead, st->file);
}

static drflac_bool32 flac_on_seek(void *pUserData, int offset, drflac_seek_origin origin) {
    flac_state_t *st = (flac_state_t*)pUserData;
    if (!st || !st->file) return DRFLAC_FALSE;
    int whence = SEEK_SET;
    if (origin == DRFLAC_SEEK_CUR) whence = SEEK_CUR;
    else if (origin == DRFLAC_SEEK_END) whence = SEEK_END;
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
        if (picSize > 0 && picSize <= 256 * 1024 && picData) {
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
    if (!st || !st->pFlac || !out || max_samples == 0) return 0;

    if (st->downmix_multichannel) {
        // Multichannel FLAC (e.g. 5.1 / 7.1) downmixed to stereo
        drflac_int32 scratch[128 * 8];
        uint8_t raw_ch = st->raw_channels;
        drflac_uint64 frames_target = (max_samples / 2 < 128) ? (max_samples / 2) : 128;
        if (frames_target == 0) return 0;

        drflac_uint64 frames_read = drflac_read_pcm_frames_s32(st->pFlac, frames_target, scratch);
        if (frames_read == 0) return 0;

        for (size_t f = 0; f < (size_t)frames_read; f++) {
            const drflac_int32 *s = &scratch[f * raw_ch];
            // Standard 5.1 downmix: L = FL + C*0.7 + SL*0.7; R = FR + C*0.7 + SR*0.7
            int32_t fl = s[0];
            int32_t fr = (raw_ch > 1) ? s[1] : s[0];
            int32_t fc = (raw_ch > 2) ? s[2] : 0;
            int32_t sl = (raw_ch > 4) ? s[4] : 0;
            int32_t sr = (raw_ch > 5) ? s[5] : 0;

            int64_t l_mix = (int64_t)fl + (fc / 2) + (sl / 2);
            int64_t r_mix = (int64_t)fr + (fc / 2) + (sr / 2);

            if (l_mix > 2147483647LL) l_mix = 2147483647LL;
            if (l_mix < -2147483648LL) l_mix = -2147483648LL;
            if (r_mix > 2147483647LL) r_mix = 2147483647LL;
            if (r_mix < -2147483648LL) r_mix = -2147483648LL;

            out[f * 2]     = (int32_t)l_mix;
            out[f * 2 + 1] = (int32_t)r_mix;
        }
        return (int)(frames_read * 2);
    }

    uint8_t ch = dec->info.channels ? dec->info.channels : 1;
    drflac_uint64 frames_to_read = max_samples / ch;
    if (frames_to_read == 0) return 0;

    drflac_uint64 frames_read = drflac_read_pcm_frames_s32(st->pFlac, frames_to_read, (drflac_int32*)out);
    if (frames_read == 0) return 0;

    return (int)(frames_read * ch);
}

static bool flac_get_cover(decoder_t *dec, uint8_t **out_data, size_t *out_size) {
    flac_state_t *st = (flac_state_t*)dec->user_data;
    if (!st || !st->cover_data || st->cover_size == 0) return false;
    *out_data = st->cover_data;
    *out_size = st->cover_size;
    st->cover_data = NULL; // transferred ownership
    st->cover_size = 0;
    return true;
}

static bool flac_seek(decoder_t *dec, uint32_t target_sec) {
    flac_state_t *st = (flac_state_t*)dec->user_data;
    if (!st || !st->pFlac) return false;
    drflac_uint64 target_frame = (drflac_uint64)target_sec * st->pFlac->sampleRate;
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

    // First attempt: open with metadata for picture extraction
    drflac *pFlac = drflac_open_with_metadata(&flac_on_read, &flac_on_seek, &flac_on_tell, &flac_on_meta, st, NULL);
    if (!pFlac) {
        // Fallback attempt: rewind and open without metadata parsing
        hal_fseek(f, 0, SEEK_SET);
        pFlac = drflac_open(&flac_on_read, &flac_on_seek, &flac_on_tell, st, NULL);
    }

    if (!pFlac) {
        if (st->cover_data) free(st->cover_data);
        free(st);
        hal_fclose(f);
        return NULL;
    }
    st->pFlac = pFlac;
    st->raw_channels = (uint8_t)pFlac->channels;
    st->downmix_multichannel = (pFlac->channels > 2);

    uint32_t sample_rate = pFlac->sampleRate;
    uint8_t channels = st->downmix_multichannel ? 2 : (uint8_t)pFlac->channels;
    uint8_t bits_per_sample = (uint8_t)pFlac->bitsPerSample;
    uint32_t duration_secs = (sample_rate > 0) ? (uint32_t)(pFlac->totalPCMFrameCount / sample_rate) : 0;

    decoder_t *dec = (decoder_t*)calloc(1, sizeof(decoder_t));
    if (!dec) {
        drflac_close(pFlac);
        if (st->cover_data) free(st->cover_data);
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
