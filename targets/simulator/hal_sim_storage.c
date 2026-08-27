#include "hal/hal_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define SIM_SDCARD_ROOT "./sdcard"

static void translate_path(const char *in_path, char *out_path, size_t out_len) {
    if (strncmp(in_path, "/sdcard", 7) == 0) {
        snprintf(out_path, out_len, "%s%s", SIM_SDCARD_ROOT, in_path + 7);
    } else {
        snprintf(out_path, out_len, "%s", in_path);
    }
}

int hal_storage_mount(void) {
    // Create ./sdcard directory if it doesn't exist
    struct stat st;
    if (stat(SIM_SDCARD_ROOT, &st) != 0) {
        mkdir(SIM_SDCARD_ROOT, 0755);
    }
    return 0;
}

void hal_storage_unmount(void) {
}

hal_file_t* hal_fopen(const char *path, const char *mode) {
    char real_path[512];
    translate_path(path, real_path, sizeof(real_path));
    return (hal_file_t*)fopen(real_path, mode);
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

int hal_fclose(hal_file_t *file) {
    if (!file) return -1;
    return fclose((FILE*)file);
}

size_t hal_fsize(hal_file_t *file) {
    if (!file) return 0;
    FILE *f = (FILE*)file;
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, cur, SEEK_SET);
    return (sz > 0) ? (size_t)sz : 0;
}

typedef struct {
    DIR *d;
    char base_path[512];
} sim_dir_t;

hal_dir_t* hal_opendir(const char *path) {
    char real_path[512];
    translate_path(path, real_path, sizeof(real_path));

    DIR *d = opendir(real_path);
    if (!d) return NULL;

    sim_dir_t *sd = (sim_dir_t*)malloc(sizeof(sim_dir_t));
    if (!sd) {
        closedir(d);
        return NULL;
    }
    sd->d = d;
    strncpy(sd->base_path, real_path, sizeof(sd->base_path) - 1);
    sd->base_path[sizeof(sd->base_path) - 1] = '\0';
    return (hal_dir_t*)sd;
}

bool hal_readdir(hal_dir_t *dir, hal_dir_entry_t *entry) {
    if (!dir || !entry) return false;
    sim_dir_t *sd = (sim_dir_t*)dir;

    struct dirent *de = readdir(sd->d);
    if (!de) return false;

    strncpy(entry->name, de->d_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';

    char full[1024];
    snprintf(full, sizeof(full), "%s/%s", sd->base_path, de->d_name);

    struct stat st;
    if (stat(full, &st) == 0) {
        entry->is_dir = S_ISDIR(st.st_mode);
        entry->size = (size_t)st.st_size;
    } else {
        entry->is_dir = false;
        entry->size = 0;
    }
    return true;
}

void hal_closedir(hal_dir_t *dir) {
    if (!dir) return;
    sim_dir_t *sd = (sim_dir_t*)dir;
    if (sd->d) closedir(sd->d);
    free(sd);
}
