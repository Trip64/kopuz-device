#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_storage.h"
#include <stdio.h>
#include <string.h>

// mikromedia Plus for STM32F7 MicroSD Pin Mapping (SDMMC1 4-bit mode)
//   SDMMC_D0:  PC8
//   SDMMC_D1:  PC9
//   SDMMC_D2:  PC10
//   SDMMC_D3:  PC11
//   SDMMC_CMD: PD2
//   SDMMC_CLK: PC12
//   SD_DETECT: PD3 (Active Low)

int hal_storage_mount(void) {
    return 0;
}

void hal_storage_unmount(void) {
}

hal_file_t* hal_fopen(const char *path, const char *mode) {
    return (hal_file_t*)fopen(path, mode);
}

size_t hal_fread(void *ptr, size_t size, size_t count, hal_file_t *file) {
    return fread(ptr, size, count, (FILE*)file);
}

int hal_fseek(hal_file_t *file, long offset, int whence) {
    return fseek((FILE*)file, offset, whence);
}

long hal_ftell(hal_file_t *file) {
    return ftell((FILE*)file);
}

int hal_fclose(hal_file_t *file) {
    return fclose((FILE*)file);
}

size_t hal_fsize(hal_file_t *file) {
    if (!file) return 0;
    long cur = ftell((FILE*)file);
    fseek((FILE*)file, 0, SEEK_END);
    long sz = ftell((FILE*)file);
    fseek((FILE*)file, cur, SEEK_SET);
    return (size_t)sz;
}

hal_dir_t* hal_opendir(const char *path) {
    (void)path;
    return NULL;
}

bool hal_readdir(hal_dir_t *dir, hal_dir_entry_t *entry) {
    (void)dir;
    (void)entry;
    return false;
}

void hal_closedir(hal_dir_t *dir) {
    (void)dir;
}

#endif
