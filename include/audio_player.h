#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize audio subsystem and player thread/task
int audio_player_init(app_state_t *app);

// Send command to audio player
void audio_player_send_command(app_command_t cmd);

// Decode and stream audio task loop (can be called periodically or in dedicated thread)
void audio_player_process(void);

// Seek to position in current track
bool audio_player_seek(uint32_t target_sec);

// Update/decay VU meter animation (call once per UI frame ~30 FPS)
void audio_player_tick_vu(void);

// Stop and deinitialize
void audio_player_close(void);

#ifdef __cplusplus
}
#endif
