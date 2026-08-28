#include "audio_player.h"
#include "codecs/decoder.h"
#include "hal/hal_audio.h"
#include "hal/hal_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/hal_storage.h"

#define HW_STREAM_BUF_SIZE 4096
static uint8_t s_hw_buf[HW_STREAM_BUF_SIZE];
static size_t s_hw_buf_len = 0;
static size_t s_hw_buf_pos = 0;

static app_state_t *s_app = NULL;
static decoder_t *s_decoder = NULL;
static hal_file_t *s_raw_stream_file = NULL;
static bool s_is_hw_stream = false;
static int32_t s_pcm_buf[AUDIO_BUFFER_SAMPLES];

static void load_current_track(void) {
    if (s_raw_stream_file) {
        hal_fclose(s_raw_stream_file);
        s_raw_stream_file = NULL;
    }
    if (s_decoder) {
        s_decoder->close(s_decoder);
        s_decoder = NULL;
    }
    s_is_hw_stream = false;

    const track_t *track = app_get_current_track(s_app);
    if (!track) return;

    if (hal_audio_has_hardware_codec() && (strstr(track->path, ".mp3") || strstr(track->path, ".MP3") || strstr(track->path, ".wav") || strstr(track->path, ".WAV"))) {
        s_raw_stream_file = hal_fopen(track->path, "rb");
        if (s_raw_stream_file) {
            s_is_hw_stream = true;
            s_hw_buf_len = 0;
            s_hw_buf_pos = 0;
            hal_audio_init(44100, 2);
            hal_audio_set_volume(s_app->volume);

            const char *fmt_str = (strstr(track->path, ".wav") || strstr(track->path, ".WAV")) ? "WAV" : "MP3";
            snprintf(s_app->format_badge, sizeof(s_app->format_badge), "%s HW/16b", fmt_str);
            s_app->position_ms = 0;
            s_app->dirty = true;
            printf("Hardware streaming %s [%s]\n", track->path, s_app->format_badge);
            return;
        }
    }

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
    if (!s_app || s_app->state != PLAYBACK_PLAYING) {
        return;
    }

    if (s_is_hw_stream && s_raw_stream_file) {
        // While codec internal FIFO has room, push 32-byte chunks (up to 4KB burst)
        int loops = 0;
        while (hal_audio_needs_data() && loops++ < 128) {
            if (s_hw_buf_pos >= s_hw_buf_len) {
                s_hw_buf_len = hal_fread(s_hw_buf, 1, sizeof(s_hw_buf), s_raw_stream_file);
                s_hw_buf_pos = 0;
                if (s_hw_buf_len == 0) {
                    app_command_t cmd = app_on_track_end(s_app);
                    if (cmd == CMD_LOAD_CURRENT) {
                        load_current_track();
                    } else {
                        hal_audio_stop();
                        if (s_raw_stream_file) {
                            hal_fclose(s_raw_stream_file);
                            s_raw_stream_file = NULL;
                        }
                    }
                    return;
                }
            }

            size_t to_send = s_hw_buf_len - s_hw_buf_pos;
            if (to_send > 32) to_send = 32;

            size_t written = hal_audio_write_stream(s_hw_buf + s_hw_buf_pos, to_send);
            if (written == 0) break;
            s_hw_buf_pos += written;
            s_app->position_ms += (uint32_t)((written * 1000) / (128000 / 8));
        }

        if (s_app->vu_enabled && s_hw_buf_len > 0) {
            for (int b = 0; b < 8; b++) {
                uint8_t v = (s_hw_buf[(b * 32) % s_hw_buf_len] & 0x0F) + 6;
                s_app->vu_meter[b] = v;
                if (v > s_app->vu_peak[b]) s_app->vu_peak[b] = v;
            }
        }
        return;
    }

    if (!s_decoder) return;

    int n = s_decoder->decode(s_decoder, s_pcm_buf, AUDIO_BUFFER_SAMPLES);
    if (n > 0) {
        hal_audio_write(s_pcm_buf, (size_t)n);

        uint8_t ch = s_decoder->info.channels ? s_decoder->info.channels : 1;
        uint32_t sr = s_decoder->info.sample_rate ? s_decoder->info.sample_rate : 44100;
        size_t frames = (size_t)n / ch;
        s_app->position_ms += (uint32_t)(((uint64_t)frames * 1000) / sr);

        int step = 2;
        int count = 0;
        int32_t p1 = 0, p2 = 0;
        uint32_t band_acc[8] = {0};

        for (int i = 0; i < n; i += step) {
            int32_t s = s_pcm_buf[i] >> 16;
            int32_t abs_s = (s < 0) ? -s : s;
            int32_t d1 = (s - p1); if (d1 < 0) d1 = -d1;
            int32_t d2 = (s - 2 * p1 + p2); if (d2 < 0) d2 = -d2;
            p2 = p1;
            p1 = s;

            band_acc[0] += (uint32_t)abs_s;
            band_acc[1] += (uint32_t)(abs_s * 3 / 4 + d1 / 4);
            band_acc[2] += (uint32_t)(abs_s / 2 + d1 / 2);
            band_acc[3] += (uint32_t)(abs_s / 3 + d1 * 2 / 3);
            band_acc[4] += (uint32_t)d1;
            band_acc[5] += (uint32_t)(d1 * 3 / 4 + d2 / 4);
            band_acc[6] += (uint32_t)(d1 / 2 + d2 / 2);
            band_acc[7] += (uint32_t)d2;
            count++;
        }

        if (count > 0 && s_app->vu_enabled) {
            uint32_t max_e = 100;
            for (int b = 0; b < 8; b++) {
                uint32_t avg = band_acc[b] / (uint32_t)count;
                if (avg > max_e) max_e = avg;
            }
            if (max_e < 600) max_e = 600;

            for (int b = 0; b < 8; b++) {
                uint32_t avg = band_acc[b] / (uint32_t)count;
                uint8_t lvl = (uint8_t)((avg * 24) / max_e);
                if (lvl > 24) lvl = 24;
                if (lvl < 2) lvl = 2;

                s_app->vu_meter[b] = lvl;
                if (lvl > s_app->vu_peak[b]) {
                    s_app->vu_peak[b] = lvl;
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

void audio_player_tick_vu(void) {
    if (!s_app || !s_app->vu_enabled) return;

    for (int b = 0; b < 8; b++) {
        if (s_app->state != PLAYBACK_PLAYING) {
            if (s_app->vu_meter[b] > 0) s_app->vu_meter[b]--;
            if (s_app->vu_peak[b] > 0) s_app->vu_peak[b]--;
        } else {
            if (s_app->vu_meter[b] > 2) {
                s_app->vu_meter[b] -= 2;
            } else if (s_app->vu_meter[b] > 0) {
                s_app->vu_meter[b]--;
            }

            if (s_app->vu_peak[b] > s_app->vu_meter[b]) {
                s_app->vu_peak[b]--;
            }
        }
    }
}

bool audio_player_seek(uint32_t target_sec) {
    if (!s_app) return false;
    if (s_is_hw_stream && s_raw_stream_file) {
        long offset = (long)target_sec * 16000;
        hal_fseek(s_raw_stream_file, offset, SEEK_SET);
        s_app->position_ms = target_sec * 1000;
        return true;
    }
    if (!s_decoder || !s_decoder->seek) return false;

    if (s_decoder->seek(s_decoder, target_sec)) {
        s_app->position_ms = target_sec * 1000;
        return true;
    }
    return false;
}

void audio_player_close(void) {
    if (s_raw_stream_file) {
        hal_fclose(s_raw_stream_file);
        s_raw_stream_file = NULL;
    }
    if (s_decoder) {
        s_decoder->close(s_decoder);
        s_decoder = NULL;
    }
    hal_audio_close();
}
