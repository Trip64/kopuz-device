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
#if defined(TARGET_CROWPANEL_DIS03024H)
    #define UI_ART_X        10
    #define UI_ART_Y        42
    #define CROW_UI_MENU_Y          35
    #define CROW_UI_MENU_CARD_H     48
    #define CROW_UI_MENU_STEP_Y     53
    #define CROW_UI_LIST_Y          35
    #define CROW_UI_LIST_ROW_H      30
    #define CROW_UI_VISIBLE_ROWS    5
    #define CROW_UI_NAV_Y           194
    #define CROW_UI_NAV_CELL_W      80
#else
    #define UI_ART_X        4
    #define UI_ART_Y        (UI_BODY_TOP + 2)
#endif

void ui_init(framebuffer_t *fb);
void ui_render(framebuffer_t *fb, const app_state_t *app);
void ui_render_message(framebuffer_t *fb, const char *heading, const char *body);
void ui_render_bsod(framebuffer_t *fb, const char *stop_code, const char *details, const char *qr_payload);
#if defined(TARGET_CROWPANEL_DIS03024H)
btn_event_t ui_crowpanel_touch(app_state_t *app, const touch_event_t *touch);
#endif

#ifdef __cplusplus
}
#endif
