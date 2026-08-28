#include "audio_player.h"
#include "codecs/decoder.h"
#include "hal/hal_audio.h"
#include "hal/hal_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static app_state_t *s_app = NULL;
static decoder_t *s_decoder = NULL;
static int32_t s_pcm_buf[AUDIO_BUFFER_SAMPLES];

static void load_current_track(void) {
    if (s_decoder) {
        s_decoder->close(s_decoder);
        s_decoder = NULL;
    }

    const track_t *track = app_get_current_track(s_app);
    if (!track) return;

    s_decoder = decoder_open(track->path);
    if (!s_decoder) {
        printf("Failed to open audio: %s\n", track->path);
        app_trigger_bsod(s_app, "ERR_FILE_CORRUPT_OR_UNSUPPORTED", track->title);
        return;
    }

#if defined(PICO_RP2040)
    if (s_decoder->info.sample_rate > 96000) {
        char err_detail[64];
        snprintf(err_detail, sizeof(err_detail), "%uHz exceeds RP2040 ceiling", (unsigned)s_decoder->info.sample_rate);
        s_decoder->close(s_decoder);
        s_decoder = NULL;
        app_trigger_bsod(s_app, "ERR_SAMPLE_RATE_TOO_HIGH", err_detail);
        return;
    }
#endif

    if (s_decoder->info.channels == 0 || s_decoder->info.channels > 2) {
        char err_detail[64];
        snprintf(err_detail, sizeof(err_detail), "%u channels (max 2ch)", (unsigned)s_decoder->info.channels);
        s_decoder->close(s_decoder);
        s_decoder = NULL;
        app_trigger_bsod(s_app, "ERR_UNSUPPORTED_CHANNELS", err_detail);
        return;
    }

    if (s_decoder->info.sample_rate < 8000 || s_decoder->info.sample_rate > 192000) {
        char err_detail[64];
        snprintf(err_detail, sizeof(err_detail), "%u Hz out of bounds", (unsigned)s_decoder->info.sample_rate);
        s_decoder->close(s_decoder);
        s_decoder = NULL;
        app_trigger_bsod(s_app, "ERR_INVALID_SAMPLE_RATE", err_detail);
        return;
    }

    if (s_decoder->info.duration_secs > 0) {
        s_app->queue[s_app->current_index].duration_secs = s_decoder->info.duration_secs;
    }

    hal_audio_init(s_decoder->info.sample_rate, s_decoder->info.channels);
    hal_audio_set_volume(s_app->volume);

    s_app->art_valid = false;
    uint8_t *cover_bytes = NULL;
    size_t cover_sz = 0;
    if (s_decoder->get_cover(s_decoder, &cover_bytes, &cover_sz)) {
        if (decode_art_rgb565(cover_bytes, cover_sz, s_app->art_size, s_app->art_rgb565)) {
            s_app->art_valid = true;
        }
        free(cover_bytes);
    }

    const char *fmt_str = "WAV";
    if (strstr(track->path, ".flac")) fmt_str = "FLAC";
    else if (strstr(track->path, ".mp3")) fmt_str = "MP3";
    uint32_t khz = (s_decoder->info.sample_rate + 500) / 1000;
    uint8_t bps = s_decoder->info.bits_per_sample ? s_decoder->info.bits_per_sample : 16;
    snprintf(s_app->format_badge, sizeof(s_app->format_badge), "%s %ub/%uk",
             fmt_str, (unsigned)bps, (unsigned)khz);

    s_app->position_ms = 0;
    s_app->dirty = true;
    printf("Playing %s (%u Hz, %u ch, %u sec) [%s]\n",
           track->path, (unsigned)s_decoder->info.sample_rate,
           (unsigned)s_decoder->info.channels, (unsigned)s_decoder->info.duration_secs,
           s_app->format_badge);
}

int audio_player_init(app_state_t *app) {
    s_app = app;
    hal_audio_init(AUDIO_DEFAULT_SAMPLE_RATE, AUDIO_CHANNELS);
    hal_audio_set_volume(app->volume);
    return 0;
}

void audio_player_send_command(app_command_t cmd) {
    if (!s_app) return;

    switch (cmd) {
        case CMD_LOAD_CURRENT:
            load_current_track();
            break;
        case CMD_PLAY:
            hal_audio_resume();
            break;
        case CMD_PAUSE:
            hal_audio_stop();
            break;
        case CMD_SEEK_FWD: {
            uint32_t cur_sec = s_app->position_ms / 1000;
            const track_t *cur = app_get_current_track(s_app);
            uint32_t target = cur_sec + 5;
            if (cur && cur->duration_secs > 0 && target > cur->duration_secs) {
                target = cur->duration_secs;
            }
            audio_player_seek(target);
            break;
        }
        case CMD_SEEK_BACK: {
            uint32_t cur_sec = s_app->position_ms / 1000;
            uint32_t target = (cur_sec > 5) ? (cur_sec - 5) : 0;
            audio_player_seek(target);
            break;
        }
        case CMD_NONE:
        default:
            break;
    }
}

void audio_player_process(void) {
    if (!s_app || s_app->state != PLAYBACK_PLAYING || !s_decoder) {
        return;
    }

    int n = s_decoder->decode(s_decoder, s_pcm_buf, AUDIO_BUFFER_SAMPLES);
    if (n > 0) {
        hal_audio_write(s_pcm_buf, (size_t)n);

        uint8_t ch = s_decoder->info.channels ? s_decoder->info.channels : 1;
        uint32_t sr = s_decoder->info.sample_rate ? s_decoder->info.sample_rate : 44100;
        size_t frames = (size_t)n / ch;
        s_app->position_ms += (uint32_t)(((uint64_t)frames * 1000) / sr);

        int chunk = n / 8;
        if (chunk > 0) {
            for (int b = 0; b < 8; b++) {
                int start = b * chunk;
                int end = start + chunk;
                uint32_t peak = 0;
                for (int i = start; i < end; i += 4) {
                    int32_t smp = s_pcm_buf[i] >> 16;
                    if (smp < 0) smp = -smp;
                    if ((uint32_t)smp > peak) peak = (uint32_t)smp;
                }
                uint8_t lvl = (uint8_t)((peak * 12) / 32768);
                if (lvl > 12) lvl = 12;
                if (lvl >= s_app->vu_meter[b]) {
                    s_app->vu_meter[b] = lvl;
                } else if (s_app->vu_meter[b] > 0) {
                    s_app->vu_meter[b]--;
                }
            }
        }
    } else if (n == 0) {
        app_command_t cmd = app_on_track_end(s_app);
        if (cmd == CMD_LOAD_CURRENT) {
            load_current_track();
        } else {
            hal_audio_stop();
            if (s_decoder) {
                s_decoder->close(s_decoder);
                s_decoder = NULL;
            }
        }
    } else {
        printf("Decode error\n");
        hal_audio_stop();
        if (s_decoder) {
            s_decoder->close(s_decoder);
            s_decoder = NULL;
        }
        app_trigger_bsod(s_app, "ERR_STREAM_DECODE_FAILED", "Corrupt audio stream");
    }
}

bool audio_player_seek(uint32_t target_sec) {
    if (!s_decoder || !s_decoder->seek || !s_app) return false;
    hal_audio_stop();
    bool ok = s_decoder->seek(s_decoder, target_sec);
    if (ok) {
        s_app->position_ms = target_sec * 1000;
        s_app->dirty = true;
    }
    hal_audio_resume();
    return ok;
}

void audio_player_close(void) {
    if (s_decoder) {
        s_decoder->close(s_decoder);
        s_decoder = NULL;
    }
    hal_audio_stop();
    hal_audio_close();
}
