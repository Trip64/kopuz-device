#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void hal_file_t;

typedef struct {
    char name[256];
    bool is_dir;
    size_t size;
} hal_dir_entry_t;

typedef void hal_dir_t;

// Mount storage (SD card FAT32)
int hal_storage_mount(void);

// Unmount storage
void hal_storage_unmount(void);

// Open file
hal_file_t* hal_fopen(const char *path, const char *mode);

// Read from file
size_t hal_fread(void *ptr, size_t size, size_t count, hal_file_t *file);

// Write to file
size_t hal_fwrite(const void *ptr, size_t size, size_t count, hal_file_t *file);

// Seek file
int hal_fseek(hal_file_t *file, long offset, int whence);

// Tell position
long hal_ftell(hal_file_t *file);

// Close file
int hal_fclose(hal_file_t *file);

// Get total file size
size_t hal_fsize(hal_file_t *file);

// Open directory
hal_dir_t* hal_opendir(const char *path);

// Read next entry (returns true if an entry was read, false on end/error)
bool hal_readdir(hal_dir_t *dir, hal_dir_entry_t *entry);

// Close directory
void hal_closedir(hal_dir_t *dir);

#ifdef __cplusplus
}
#endif
