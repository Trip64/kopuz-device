#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    uint16_t fg; // Foreground / ink color in RGB565 (big-endian)
    uint16_t bg; // Background color in RGB565 (big-endian)
} theme_t;

static const theme_t THEMES[] = {
    {"Default",          0xFFFF, 0x0000},
    {"Gruvbox",          0xEED6, 0x2945},
    {"Gruvbox Soft",     0xFF98, 0x3185},
    {"Gruvbox Material", 0xD5F3, 0x1904},
    {"Dracula",          0xFFDE, 0x2946},
    {"Nord",             0xDEFD, 0x29A8},
    {"Catppuccin",       0xCEBE, 0x18E5},
    {"Ef Night",         0xADF7, 0x0062},
    {"Ayu Dark",         0xB595, 0x0862},
    {"Ayu Mirage",       0xCE78, 0x1926},
    {"Vague",            0xCE79, 0x10A2},
    {"One Dark",         0xAD97, 0x2966},
    {"Osmium",           0xCEBE, 0x1083},
    {"Kanagawa",         0xC658, 0x18A2},
    {"Everforest",       0xD635, 0x2166},
    {"Rose Pine",        0xE6FE, 0x18A4},
    {"Kettek16",         0xFFA7, 0x0841},
    {"Light",            0x1947, 0xFFDF},
    {"Latte",            0x4A6D, 0xEF9E},
    {"Rose Pine Dawn",   0x528F, 0xFFBD},
    {"Everforest Light", 0x5B4E, 0xFFBC},
    {"Ayu Light",        0x5B0C, 0xFFDF},
    {"One Light",        0x39C8, 0xFFDF},
    {"Gruvbox Light",    0x39C6, 0xF737},
};

#define THEMES_COUNT (sizeof(THEMES) / sizeof(THEMES[0]))

#ifdef __cplusplus
}
#endif
