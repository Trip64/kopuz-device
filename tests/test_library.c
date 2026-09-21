#include "app.h"
#include "library/library.h"
#include "settings.h"
#include "hal/hal_audio.h"
#include "hal/hal_display.h"
#include "hal/hal_system.h"

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

bool settings_save(const app_state_t *app) { return app != NULL; }
bool settings_load(app_state_t *app) { return app != NULL; }
void hal_audio_set_volume(uint8_t volume) { (void)volume; }
void hal_audio_stop(void) {}
void hal_audio_resume(void) {}
void hal_display_set_brightness(uint8_t pct) { (void)pct; }
void hal_display_set_theme(uint16_t fg, uint16_t bg) { (void)fg; (void)bg; }
uint32_t hal_system_random(void) { return 1; }
void hal_system_reboot(void) {}

static void make_dir(const char *path) {
    CHECK(mkdir(path, 0755) == 0);
}

static void make_file(const char *path) {
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fclose(file) == 0);
}

int main(void) {
    make_dir("sdcard");
    make_dir("sdcard/Artist B");
    make_dir("sdcard/Artist B/Album Z");
    make_dir("sdcard/artist a");
    make_dir("sdcard/artist a/Album B");
    make_dir("sdcard/artist a/album a");
    make_file("sdcard/Artist B/Album Z/03 - Last.WAV");
    make_file("sdcard/artist a/Album B/01 - First.MP3");
    make_file("sdcard/artist a/album a/02 - Middle.flac");
    make_file("sdcard/ignored.txt");
    make_file("sdcard/.hidden.mp3");

    app_state_t app;
    app_init(&app);
    CHECK(library_scan("/sdcard", &app) == 3);
    CHECK(app.queue_len == 3);
    CHECK(strcmp(app.queue[0].title, "02 - Middle") == 0);
    CHECK(strcmp(app.queue[1].title, "01 - First") == 0);
    CHECK(strcmp(app.queue[2].title, "03 - Last") == 0);
    CHECK(app.albums_len == 3);
    CHECK(app.artists_len == 2);
    CHECK(app.play_order_len == 3);
    CHECK(app.screen != SCREEN_BSOD);

    app_deinit(&app);

    CHECK(remove("sdcard/Artist B/Album Z/03 - Last.WAV") == 0);
    CHECK(remove("sdcard/artist a/Album B/01 - First.MP3") == 0);
    CHECK(remove("sdcard/artist a/album a/02 - Middle.flac") == 0);
    CHECK(remove("sdcard/ignored.txt") == 0);
    CHECK(remove("sdcard/.hidden.mp3") == 0);
    CHECK(rmdir("sdcard/Artist B/Album Z") == 0);
    CHECK(rmdir("sdcard/Artist B") == 0);
    CHECK(rmdir("sdcard/artist a/Album B") == 0);
    CHECK(rmdir("sdcard/artist a/album a") == 0);
    CHECK(rmdir("sdcard/artist a") == 0);
    CHECK(rmdir("sdcard") == 0);

    puts("library scan tests passed");
    return 0;
}
