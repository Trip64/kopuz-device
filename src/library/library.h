#pragma once

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

// Scan directory recursively for audio files and populate app queue and album/artist groupings
uint16_t library_scan(const char *root_path, app_state_t *app);

#ifdef __cplusplus
}
#endif
