#include "themes.h"

const theme_t* theme_get(size_t index) {
    if (index >= THEMES_COUNT) {
        index = 0;
    }
    return &THEMES[index];
}

size_t theme_get_count(void) {
    return THEMES_COUNT;
}
