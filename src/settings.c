#include "settings.h"
#include "hal/hal_storage.h"
#include "hal/hal_display.h"
#include "hal/hal_audio.h"
#include "themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SETTINGS_SD_CONFIG_FILE  STORAGE_MOUNT_POINT "/kopuz.cfg"
#define SETTINGS_EEPROM_FILE     "./eeprom.bin"

#define EEPROM_MAGIC 0x4B505A31 // "KPZ1"

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t volume;
    uint8_t brightness;
    uint8_t theme_index;
    uint8_t shuffle;
    uint8_t repeat;
    uint8_t vu_enabled;
    uint8_t output_mode;
    uint8_t config_store;
    uint16_t checksum;
} __attribute__((packed)) kopuz_eeprom_data_t;

static uint16_t calc_checksum(const kopuz_eeprom_data_t *d) {
    const uint8_t *p = (const uint8_t*)d;
    size_t sz = sizeof(kopuz_eeprom_data_t) - sizeof(d->checksum);
    uint16_t sum = 0xAA55;
    for (size_t i = 0; i < sz; i++) {
        sum = (uint16_t)((sum << 1) ^ p[i]);
    }
    return sum;
}

static bool eeprom_write_block(const kopuz_eeprom_data_t *d) {
    FILE *f = fopen(SETTINGS_EEPROM_FILE, "wb");
    if (!f) return false;
    size_t written = fwrite(d, 1, sizeof(*d), f);
    fclose(f);
    return (written == sizeof(*d));
}

static bool eeprom_read_block(kopuz_eeprom_data_t *d) {
    FILE *f = fopen(SETTINGS_EEPROM_FILE, "rb");
    if (!f) return false;
    size_t rd = fread(d, 1, sizeof(*d), f);
    fclose(f);
    if (rd != sizeof(*d)) return false;
    if (d->magic != EEPROM_MAGIC) return false;
    if (d->checksum != calc_checksum(d)) return false;
    return true;
}

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

static bool save_to_sd(const app_state_t *app) {
    hal_file_t *f = hal_fopen(SETTINGS_SD_CONFIG_FILE, "wb");
    if (!f) {
        printf("[SETTINGS] Could not open %s for writing\n", SETTINGS_SD_CONFIG_FILE);
        return false;
    }

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "# Kopuz Device Configuration\n"
        "storage=sd\n"
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
    printf("[SETTINGS] Saved configuration to SD card (%s)\n", SETTINGS_SD_CONFIG_FILE);
    return true;
}

static bool load_from_sd(app_state_t *app) {
    hal_file_t *f = hal_fopen(SETTINGS_SD_CONFIG_FILE, "rb");
    if (!f) return false;

    char line[128];
    while (1) {
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
        } else if (strcasecmp(key, "storage") == 0) {
            if (strcasecmp(val, "sd") == 0 || strcasecmp(val, "sdcard") == 0) {
                app->config_store = CONFIG_STORE_SD;
            } else {
                app->config_store = CONFIG_STORE_EEPROM;
            }
        }
    }

    hal_fclose(f);
    printf("[SETTINGS] Loaded configuration from SD card\n");
    return true;
}

bool settings_save(const app_state_t *app) {
    if (!app) return false;

    // 1. Save to EEPROM (NVS)
    kopuz_eeprom_data_t ep = {
        .magic = EEPROM_MAGIC,
        .version = 1,
        .volume = app->volume,
        .brightness = app->brightness,
        .theme_index = app->theme_index,
        .shuffle = app->shuffle ? 1 : 0,
        .repeat = (uint8_t)app->repeat,
        .vu_enabled = app->vu_enabled ? 1 : 0,
        .output_mode = (uint8_t)app->output_mode,
        .config_store = (uint8_t)app->config_store,
        .checksum = 0
    };
    ep.checksum = calc_checksum(&ep);
    bool ep_ok = eeprom_write_block(&ep);
    if (ep_ok) {
        printf("[SETTINGS] Saved configuration to EEPROM\n");
    }

    // 2. If user selected SD Card storage, also save / update /sdcard/kopuz.cfg
    if (app->config_store == CONFIG_STORE_SD) {
        save_to_sd(app);
    }

    return true;
}

bool settings_load(app_state_t *app) {
    if (!app) return false;

    bool loaded = false;
    kopuz_eeprom_data_t ep;

    // 1. Attempt loading from EEPROM / NVS first (Default)
    if (eeprom_read_block(&ep)) {
        if (ep.volume <= 100) app->volume = ep.volume;
        if (ep.brightness >= 10 && ep.brightness <= 100) app->brightness = ep.brightness;
        if ((size_t)ep.theme_index < THEMES_COUNT) app->theme_index = ep.theme_index;
        app->shuffle = (ep.shuffle != 0);
        if (ep.repeat <= 2) app->repeat = (repeat_mode_t)ep.repeat;
        app->vu_enabled = (ep.vu_enabled != 0);
        if (ep.output_mode <= 1) app->output_mode = (audio_output_mode_t)ep.output_mode;
        app->config_store = (ep.config_store == CONFIG_STORE_SD) ? CONFIG_STORE_SD : CONFIG_STORE_EEPROM;
        loaded = true;
        printf("[SETTINGS] Loaded configuration from EEPROM (vol=%u, bright=%u, theme=%u, store=%s)\n",
               (unsigned)app->volume, (unsigned)app->brightness, (unsigned)app->theme_index,
               (app->config_store == CONFIG_STORE_EEPROM) ? "EEPROM" : "SD");
    }

    // 2. If storage is set to SD or EEPROM was not initialized, check SD card
    if (app->config_store == CONFIG_STORE_SD || !loaded) {
        if (load_from_sd(app)) {
            loaded = true;
        }
    }

    // 3. If no config existed in either place, save current defaults to EEPROM
    if (!loaded) {
        app->config_store = CONFIG_STORE_EEPROM;
        printf("[SETTINGS] No saved configuration found, initializing defaults in EEPROM...\n");
        settings_save(app);
    }

    // Apply loaded parameters to hardware
    hal_audio_set_volume(app->volume);
    hal_display_set_brightness(app->brightness);
    hal_display_set_theme(THEMES[app->theme_index].fg, THEMES[app->theme_index].bg);
    return true;
}
