#include "app.h"
#include "settings.h"
#include "hal/hal_audio.h"
#include "hal/hal_display.h"
#include "hal/hal_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static uint8_t s_applied_volume;
static uint8_t s_applied_brightness;

void hal_audio_set_volume(uint8_t volume) { s_applied_volume = volume; }
void hal_display_set_brightness(uint8_t pct) { s_applied_brightness = pct; }
void hal_display_set_theme(uint16_t fg, uint16_t bg) { (void)fg; (void)bg; }

static const char *translate_path(const char *path) {
    return strcmp(path, "/sdcard/kopuz.cfg") == 0 ? "sdcard/kopuz.cfg" : path;
}

hal_file_t *hal_fopen(const char *path, const char *mode) {
    return (hal_file_t*)fopen(translate_path(path), mode);
}
size_t hal_fread(void *ptr, size_t size, size_t count, hal_file_t *file) {
    return fread(ptr, size, count, (FILE*)file);
}
size_t hal_fwrite(const void *ptr, size_t size, size_t count, hal_file_t *file) {
    return fwrite(ptr, size, count, (FILE*)file);
}
int hal_fclose(hal_file_t *file) { return fclose((FILE*)file); }

int main(void) {
    CHECK(mkdir("sdcard", 0755) == 0);
    remove("eeprom.bin");

    app_state_t saved;
    memset(&saved, 0, sizeof(saved));
    saved.volume = 35;
    saved.brightness = 80;
    saved.theme_index = 3;
    saved.shuffle = true;
    saved.repeat = REPEAT_ONE;
    saved.vu_enabled = false;
    saved.output_mode = OUTPUT_I2S_DAC;
    saved.config_store = CONFIG_STORE_EEPROM;
    CHECK(settings_save(&saved));

    app_state_t loaded;
    memset(&loaded, 0, sizeof(loaded));
    loaded.volume = 70;
    loaded.brightness = 100;
    loaded.vu_enabled = true;
    CHECK(settings_load(&loaded));
    CHECK(loaded.volume == 35);
    CHECK(loaded.brightness == 80);
    CHECK(loaded.theme_index == 3);
    CHECK(loaded.shuffle);
    CHECK(loaded.repeat == REPEAT_ONE);
    CHECK(!loaded.vu_enabled);
    CHECK(s_applied_volume == 35);
    CHECK(s_applied_brightness == 80);

    saved.config_store = CONFIG_STORE_SD;
    saved.volume = 55;
    saved.brightness = 60;
    CHECK(settings_save(&saved));
    remove("eeprom.bin");

    memset(&loaded, 0, sizeof(loaded));
    loaded.volume = 70;
    loaded.brightness = 100;
    loaded.vu_enabled = true;
    CHECK(settings_load(&loaded));
    CHECK(loaded.config_store == CONFIG_STORE_SD);
    CHECK(loaded.volume == 55);
    CHECK(loaded.brightness == 60);

    CHECK(remove("sdcard/kopuz.cfg") == 0);
    CHECK(rmdir("sdcard") == 0);
    puts("settings tests passed");
    return 0;
}
