#pragma once

#include "framebuffer.h"
#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

// Layout bounds
#if (LCD_HEIGHT <= 64)
    #define UI_HEADER_Y         9
    #define UI_BODY_TOP         11
    #define UI_ROW_HEIGHT       11
    #define UI_FOOTER_HEIGHT    12
#elif (LCD_HEIGHT <= 128)
    #define UI_HEADER_Y         12
    #define UI_BODY_TOP         14
    #define UI_ROW_HEIGHT       12
    #define UI_FOOTER_HEIGHT    18
#else
    #define UI_HEADER_Y         14
    #define UI_BODY_TOP         16
    #define UI_ROW_HEIGHT       13
    #define UI_FOOTER_HEIGHT    24
#endif

#define UI_FOOTER_Y         (LCD_HEIGHT - UI_FOOTER_HEIGHT)
#define UI_VISIBLE_ROWS     ((UI_FOOTER_Y - UI_BODY_TOP) / UI_ROW_HEIGHT)

// Album art origin in landscape coords
#define UI_ART_X            4
#define UI_ART_Y            (UI_BODY_TOP + 2)

void ui_init(framebuffer_t *fb);
void ui_render(framebuffer_t *fb, const app_state_t *app);
void ui_render_message(framebuffer_t *fb, const char *heading, const char *body);
void ui_render_bsod(framebuffer_t *fb, const char *stop_code, const char *details, const char *qr_payload);

#ifdef __cplusplus
}
#endif
