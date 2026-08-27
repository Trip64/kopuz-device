#include "decoder.h"
#include <string.h>
#include <ctype.h>

extern decoder_t* wav_decoder_open(const char *path);
extern decoder_t* mp3_decoder_open(const char *path);
extern decoder_t* flac_decoder_open(const char *path);

decoder_t* decoder_open(const char *path) {
    if (!path) return NULL;

    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;

    char ext[16] = {0};
    size_t i = 0;
    dot++; // skip '.'
    while (dot[i] && i < sizeof(ext) - 1) {
        ext[i] = (char)tolower((unsigned char)dot[i]);
        i++;
    }
    ext[i] = '\0';

    if (strcmp(ext, "wav") == 0) {
        return wav_decoder_open(path);
    } else if (strcmp(ext, "mp3") == 0) {
        return mp3_decoder_open(path);
    } else if (strcmp(ext, "flac") == 0) {
        return flac_decoder_open(path);
    }

    return NULL;
}
