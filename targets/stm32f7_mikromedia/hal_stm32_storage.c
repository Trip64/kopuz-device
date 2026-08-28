#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_storage.h"
#include "ff.h"
#include "sdcard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static FATFS s_fatfs;
static bool s_mounted = false;

typedef struct {
    FIL fil;
} hal_file_impl_t;

typedef struct {
    DIR dp;
} hal_dir_impl_t;

int hal_storage_mount(void) {
    if (s_mounted) return 0;
    
    if (SD_Init() != SD_OK) {
        return -1;
    }
    
    FRESULT res = f_mount(&s_fatfs, "", 1);
    if (res == FR_OK) {
        s_mounted = true;
        return 0;
    }
    return -1;
}

void hal_storage_unmount(void) {
    if (s_mounted) {
        f_mount(NULL, "", 1);
        s_mounted = false;
    }
}

static const char* normalize_path(const char *path) {
    if (!path) return "";
    while (*path == ' ') path++;
    if (strncmp(path, "/sdcard", 7) == 0) {
        path += 7;
    } else if (strncmp(path, "sdcard", 6) == 0) {
        path += 6;
    } else if (strncmp(path, "/sd", 3) == 0) {
        path += 3;
    } else if (strncmp(path, "sd", 2) == 0) {
        path += 2;
    }
    while (*path == '/') path++;
    return path;
}

hal_file_t* hal_fopen(const char *path, const char *mode) {
    if (!s_mounted && hal_storage_mount() != 0) return NULL;

    const char *norm = normalize_path(path);
    BYTE flags = FA_READ;
    if (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')) {
        flags |= FA_WRITE;
    }

    hal_file_impl_t *f = (hal_file_impl_t*)malloc(sizeof(hal_file_impl_t));
    if (!f) return NULL;

    FRESULT res = f_open(&f->fil, norm, flags);
    if (res != FR_OK) {
        free(f);
        return NULL;
    }
    return (hal_file_t*)f;
}

size_t hal_fread(void *ptr, size_t size, size_t count, hal_file_t *file) {
    if (!file || !ptr || size == 0 || count == 0) return 0;
    hal_file_impl_t *f = (hal_file_impl_t*)file;
    UINT bytes_to_read = (UINT)(size * count);
    UINT bytes_read = 0;
    FRESULT res = f_read(&f->fil, ptr, bytes_to_read, &bytes_read);
    if (res != FR_OK) return 0;
    return bytes_read / size;
}

int hal_fseek(hal_file_t *file, long offset, int whence) {
    if (!file) return -1;
    hal_file_impl_t *f = (hal_file_impl_t*)file;
    FSIZE_t target = 0;
    if (whence == SEEK_SET) {
        target = (FSIZE_t)offset;
    } else if (whence == SEEK_CUR) {
        target = f_tell(&f->fil) + offset;
    } else if (whence == SEEK_END) {
        target = f_size(&f->fil) + offset;
    }
    FRESULT res = f_lseek(&f->fil, target);
    return (res == FR_OK) ? 0 : -1;
}

long hal_ftell(hal_file_t *file) {
    if (!file) return -1;
    hal_file_impl_t *f = (hal_file_impl_t*)file;
    return (long)f_tell(&f->fil);
}

int hal_fclose(hal_file_t *file) {
    if (!file) return -1;
    hal_file_impl_t *f = (hal_file_impl_t*)file;
    f_close(&f->fil);
    free(f);
    return 0;
}

size_t hal_fsize(hal_file_t *file) {
    if (!file) return 0;
    hal_file_impl_t *f = (hal_file_impl_t*)file;
    return (size_t)f_size(&f->fil);
}

hal_dir_t* hal_opendir(const char *path) {
    if (!s_mounted && hal_storage_mount() != 0) return NULL;

    const char *norm = normalize_path(path);
    hal_dir_impl_t *d = (hal_dir_impl_t*)malloc(sizeof(hal_dir_impl_t));
    if (!d) return NULL;

    FRESULT res = f_opendir(&d->dp, norm);
    if (res != FR_OK) {
        free(d);
        return NULL;
    }
    return (hal_dir_t*)d;
}

bool hal_readdir(hal_dir_t *dir, hal_dir_entry_t *entry) {
    if (!dir || !entry) return false;
    hal_dir_impl_t *d = (hal_dir_impl_t*)dir;

    FILINFO fno;
    FRESULT res = f_readdir(&d->dp, &fno);
    if (res != FR_OK || fno.fname[0] == 0) {
        return false;
    }

    strncpy(entry->name, fno.fname, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->is_dir = (fno.fattrib & AM_DIR) ? true : false;
    entry->size = (size_t)fno.fsize;
    return true;
}

void hal_closedir(hal_dir_t *dir) {
    if (!dir) return;
    hal_dir_impl_t *d = (hal_dir_impl_t*)dir;
    f_closedir(&d->dp);
    free(d);
}

#endif
