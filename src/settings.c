#include "settings.h"
#include "hal/hal_storage.h"
#include "hal/hal_display.h"
#include "hal/hal_audio.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SETTINGS_CONFIG_FILE  STORAGE_MOUNT_POINT "/kopuz.cfg"

static void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

bool settings_save(const app_state_t *app) {
    if (!app) return false;

    hal_file_t *f = hal_fopen(SETTINGS_CONFIG_FILE, "wb");
    if (!f) {
        printf("[SETTINGS] Could not open %s for writing\n", SETTINGS_CONFIG_FILE);
        return false;
    }

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "# Kopuz Device Configuration\n"
        "volume=%u\n"
        "brightness=%u\n"
        "theme=%u\n"
        "shuffle=%u\n"
        "repeat=%u\n"
        "visualizer=%u\n"
        "output=%u\n",
        (unsigned)app->volume,
        (unsigned)app->brightness,
        (unsigned)app->theme_index,
        app->shuffle ? 1 : 0,
        (unsigned)app->repeat,
        app->vu_enabled ? 1 : 0,
        (unsigned)app->output_mode
    );

    if (len > 0) {
        hal_fwrite(buf, 1, (size_t)len, f);
    }

    hal_fclose(f);
    printf("[SETTINGS] Saved configuration to %s\n", SETTINGS_CONFIG_FILE);
    return true;
}

bool settings_load(app_state_t *app) {
    if (!app) return false;

    hal_file_t *f = hal_fopen(SETTINGS_CONFIG_FILE, "rb");
    if (!f) {
        printf("[SETTINGS] No config found at %s, generating defaults...\n", SETTINGS_CONFIG_FILE);
        return settings_save(app);
    }

    char line[128];
    while (1) {
        // Read line by line
        size_t idx = 0;
        char ch;
        while (idx < sizeof(line) - 1) {
            if (hal_fread(&ch, 1, 1, f) != 1) break;
            if (ch == '\n' || ch == '\r') {
                if (idx > 0) break;
                else continue;
            }
            line[idx++] = ch;
        }
        line[idx] = '\0';
        if (idx == 0) break;

        trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcasecmp(key, "volume") == 0) {
            int v = atoi(val);
            if (v >= 0 && v <= 100) app->volume = (uint8_t)v;
        } else if (strcasecmp(key, "brightness") == 0) {
            int b = atoi(val);
            if (b >= 10 && b <= 100) app->brightness = (uint8_t)b;
        } else if (strcasecmp(key, "theme") == 0) {
            int t = atoi(val);
            if (t >= 0 && (size_t)t < THEMES_COUNT) app->theme_index = (uint8_t)t;
        } else if (strcasecmp(key, "shuffle") == 0) {
            app->shuffle = (atoi(val) != 0);
        } else if (strcasecmp(key, "repeat") == 0) {
            int r = atoi(val);
            if (r >= 0 && r <= 2) app->repeat = (repeat_mode_t)r;
        } else if (strcasecmp(key, "visualizer") == 0 || strcasecmp(key, "vu") == 0) {
            app->vu_enabled = (atoi(val) != 0);
        } else if (strcasecmp(key, "output") == 0) {
            int o = atoi(val);
            if (o >= 0 && o <= 1) app->output_mode = (audio_output_mode_t)o;
        }
    }

    hal_fclose(f);

    // Apply loaded parameters to hardware
    hal_audio_set_volume(app->volume);
    hal_display_set_brightness(app->brightness);
    hal_display_set_theme(THEMES[app->theme_index].fg, THEMES[app->theme_index].bg);

    printf("[SETTINGS] Loaded configuration (vol=%u, bright=%u, theme=%u, shuf=%u, rpt=%u, vu=%u)\n",
           (unsigned)app->volume, (unsigned)app->brightness, (unsigned)app->theme_index,
           app->shuffle, (unsigned)app->repeat, app->vu_enabled);
    return true;
}
