#pragma once

#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load configuration from SD card (e.g. /sdcard/kopuz.cfg).
// If the config file does not exist, generates a default configuration file.
bool settings_load(app_state_t *app);

// Save current settings to SD card (/sdcard/kopuz.cfg).
bool settings_save(const app_state_t *app);

#ifdef __cplusplus
}
#endif
