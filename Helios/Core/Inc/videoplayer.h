/**
 ******************************************************************************
 * @file           : videoplayer.h
 * @brief          : Bad Apple Video Player Header
 ******************************************************************************
 */
#ifndef __VIDEOPLAYER_H
#define __VIDEOPLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Video file format (must match converter output) */
#pragma pack(push, 1)
typedef struct {
    char     magic[4];      /* "BAPV" */
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint16_t fps;
    uint8_t  reserved[2];
} VideoHeader;
#pragma pack(pop)

/* Player status */
typedef enum {
    VIDEO_OK = 0,
    VIDEO_ERROR_FILE,
    VIDEO_ERROR_FORMAT,
    VIDEO_ERROR_MEMORY,
    VIDEO_ERROR_SD
} VideoStatus;

/* Functions */
VideoStatus Video_Init(const char *filename);
VideoStatus Video_Play(void);
void        Video_Stop(void);
uint32_t    Video_GetCurrentFrame(void);
uint32_t    Video_GetTotalFrames(void);

#ifdef __cplusplus
}
#endif

#endif /* __VIDEOPLAYER_H */
