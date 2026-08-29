#if defined(ESP_PLATFORM)

#include "hal/hal_storage.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#define PIN_SD_MISO 13
#define PIN_SD_MOSI 11
#define PIN_SD_CLK  12
#define PIN_SD_CS   10

static sdmmc_card_t *s_card = NULL;

int hal_storage_mount(void) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_SD_CS;
    slot_config.host_id = host.slot;

    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &s_card);
    return (ret == ESP_OK) ? 0 : -1;
}

void hal_storage_unmount(void) {
    if (s_card) {
        esp_vfs_fat_sdcard_unmount("/sdcard", s_card);
        s_card = NULL;
    }
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

size_t hal_fwrite(const void *ptr, size_t size, size_t count, hal_file_t *file) {
    if (!file || !ptr) return 0;
    return fwrite(ptr, size, count, (FILE*)file);
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
    DIR *d = opendir(path);
    return (hal_dir_t*)d;
}

bool hal_readdir(hal_dir_t *dir, hal_dir_entry_t *entry) {
    if (!dir || !entry) return false;
    struct dirent *de = readdir((DIR*)dir);
    if (!de) return false;

    snprintf(entry->name, sizeof(entry->name), "%s", de->d_name);
    entry->is_dir = (de->d_type == DT_DIR);
    entry->size = 0;
    return true;
}

void hal_closedir(hal_dir_t *dir) {
    if (dir) closedir((DIR*)dir);
}

#endif
