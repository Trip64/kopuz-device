#include "hal/hal_audio.h"
#include <SDL.h>
#include <stdio.h>

static SDL_AudioDeviceID s_dev = 0;
static uint32_t s_rate = 44100;
static uint8_t s_channels = 2;
static uint8_t s_volume = 70;
static bool s_running = false;

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    if (s_dev != 0 && s_rate == sample_rate && s_channels == channels) {
        SDL_ClearQueuedAudio(s_dev);
        SDL_PauseAudioDevice(s_dev, 0);
        s_running = true;
        return 0;
    }

    if (s_dev != 0) {
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
    }

    s_rate = sample_rate ? sample_rate : 44100;
    s_channels = channels ? channels : 2;

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = (int)s_rate;
    want.format = AUDIO_S16SYS;
    want.channels = s_channels;
    want.samples = 1024;

    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_dev == 0) {
        printf("SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_PauseAudioDevice(s_dev, 0);
    s_running = true;
    return 0;
}

bool hal_audio_needs_data(void) {
    if (s_dev == 0 || !s_running) return false;
    uint32_t target_queued = (s_rate * s_channels * sizeof(int16_t)) / 4;
    return (SDL_GetQueuedAudioSize(s_dev) < target_queued);
}

uint32_t hal_audio_get_queued_bytes(void) {
    return s_dev ? SDL_GetQueuedAudioSize(s_dev) : 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (s_dev == 0 || !samples || sample_count == 0) return 0;

    int16_t s16_buf[1024];
    size_t done = 0;

    while (done < sample_count) {
        size_t chunk = sample_count - done;
        if (chunk > sizeof(s16_buf) / sizeof(s16_buf[0])) {
            chunk = sizeof(s16_buf) / sizeof(s16_buf[0]);
        }

        for (size_t i = 0; i < chunk; i++) {
            int32_t s = samples[done + i] >> 16;
            int32_t scaled = (s * (int32_t)s_volume) / 100;
            if (scaled > 32767) scaled = 32767;
            else if (scaled < -32768) scaled = -32768;
            s16_buf[i] = (int16_t)scaled;
        }

        SDL_QueueAudio(s_dev, s16_buf, (Uint32)(chunk * sizeof(int16_t)));
        done += chunk;
    }

    return sample_count;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = volume > 100 ? 100 : volume;
}

void hal_audio_stop(void) {
    if (s_dev != 0) {
        SDL_ClearQueuedAudio(s_dev);
        SDL_PauseAudioDevice(s_dev, 1);
        s_running = false;
    }
}

void hal_audio_resume(void) {
    if (s_dev != 0) {
        SDL_PauseAudioDevice(s_dev, 0);
        s_running = true;
    }
}

void hal_audio_close(void) {
    if (s_dev != 0) {
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
        s_running = false;
    }
}

void hal_audio_beep(uint16_t freq_hz, uint16_t duration_ms) {
    (void)freq_hz;
    (void)duration_ms;
}
