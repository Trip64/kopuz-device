#include "hal/hal_storage.h"

#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int hal_storage_mount(void) {
    return 0;
}

void hal_storage_unmount(void) {
}

hal_file_t* hal_fopen(const char *path, const char *mode) {
    FILE *f = fopen(path, mode);
    return (hal_file_t*)f;
}

int hal_fclose(hal_file_t *file) {
    if (file) return fclose((FILE*)file);
    return 0;
}

size_t hal_fread(void *ptr, size_t size, size_t count, hal_file_t *file) {
    if (!file) return 0;
    return fread(ptr, size, count, (FILE*)file);
}

int hal_fseek(hal_file_t *file, long offset, int whence) {
    if (!file) return -1;
    return fseek((FILE*)file, offset, whence);
}

long hal_ftell(hal_file_t *file) {
    if (!file) return -1;
    return ftell((FILE*)file);
}

size_t hal_fsize(hal_file_t *file) {
    if (!file) return 0;
    long cur = ftell((FILE*)file);
    fseek((FILE*)file, 0, SEEK_END);
    long sz = ftell((FILE*)file);
    fseek((FILE*)file, cur, SEEK_SET);
    return (sz > 0) ? (size_t)sz : 0;
}

hal_dir_t* hal_opendir(const char *path) {
    (void)path;
    return NULL;
}

bool hal_readdir(hal_dir_t *dir, hal_dir_entry_t *entry) {
    (void)dir; (void)entry;
    return false;
}

void hal_closedir(hal_dir_t *dir) {
    (void)dir;
}

#endif
