#if defined(ESP_PLATFORM)

#include "hal/hal_storage.h"
#include "config.h"
#include "crowpanel_pins.h"

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SD_CLOCK_KHZ 4000

static const char *TAG = "crow_sd";
static sdmmc_card_t *s_card = NULL;
static bool s_bus_initialized = false;

typedef struct {
    DIR *directory;
    char path[MAX_PATH_LEN];
} crowpanel_directory_t;

int hal_storage_mount(void) {
    if (s_card) return 0;

    /* GPIO5 is also a boot strapping pin, so actively keep SD deselected. */
    gpio_config_t cs_config = {
        .pin_bit_mask = 1ULL << CROW_SD_CS,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_config));
    gpio_set_level(CROW_SD_CS, 1);

    gpio_set_pull_mode(CROW_SD_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CROW_SD_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(CROW_SD_SCLK, GPIO_PULLUP_ONLY);

    spi_bus_config_t bus_config = {
        .mosi_io_num = CROW_SD_MOSI,
        .miso_io_num = CROW_SD_MISO,
        .sclk_io_num = CROW_SD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t error = spi_bus_initialize(CROW_SD_HOST, &bus_config, SDSPI_DEFAULT_DMA);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "SD SPI bus initialization failed: %s", esp_err_to_name(error));
        return -1;
    }
    s_bus_initialized = true;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = CROW_SD_HOST;
    host.max_freq_khz = SD_CLOCK_KHZ;

    sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    device_config.host_id = CROW_SD_HOST;
    device_config.gpio_cs = CROW_SD_CS;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    error = esp_vfs_fat_sdspi_mount(STORAGE_MOUNT_POINT, &host, &device_config,
                                    &mount_config, &s_card);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed at %d kHz: %s", SD_CLOCK_KHZ,
                 esp_err_to_name(error));
        if (error == ESP_FAIL) {
            ESP_LOGE(TAG, "Use FAT32 (not exFAT), check card seating, then power-cycle");
        }
        spi_bus_free(CROW_SD_HOST);
        s_bus_initialized = false;
        return -1;
    }

    ESP_LOGI(TAG, "SD mounted on VSPI at %d kHz", SD_CLOCK_KHZ);
    sdmmc_card_print_info(stdout, s_card);
    return 0;
}

void hal_storage_unmount(void) {
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(STORAGE_MOUNT_POINT, s_card);
        s_card = NULL;
    }
    if (s_bus_initialized) {
        spi_bus_free(CROW_SD_HOST);
        s_bus_initialized = false;
    }
}

hal_file_t *hal_fopen(const char *path, const char *mode) {
    return (hal_file_t *)fopen(path, mode);
}

int hal_fclose(hal_file_t *file) {
    return file ? fclose((FILE *)file) : 0;
}

size_t hal_fread(void *pointer, size_t size, size_t count, hal_file_t *file) {
    return file ? fread(pointer, size, count, (FILE *)file) : 0;
}

size_t hal_fwrite(const void *pointer, size_t size, size_t count, hal_file_t *file) {
    return (file && pointer) ? fwrite(pointer, size, count, (FILE *)file) : 0;
}

int hal_fseek(hal_file_t *file, long offset, int origin) {
    return file ? fseek((FILE *)file, offset, origin) : -1;
}

long hal_ftell(hal_file_t *file) {
    return file ? ftell((FILE *)file) : -1;
}

size_t hal_fsize(hal_file_t *file) {
    if (!file) return 0;
    FILE *stream = (FILE *)file;
    long position = ftell(stream);
    if (position < 0 || fseek(stream, 0, SEEK_END) != 0) return 0;
    long size = ftell(stream);
    if (fseek(stream, position, SEEK_SET) != 0) return 0;
    return size > 0 ? (size_t)size : 0;
}

hal_dir_t *hal_opendir(const char *path) {
    if (!path) return NULL;
    crowpanel_directory_t *wrapper = calloc(1, sizeof(*wrapper));
    if (!wrapper) return NULL;

    wrapper->directory = opendir(path);
    if (!wrapper->directory) {
        free(wrapper);
        return NULL;
    }
    snprintf(wrapper->path, sizeof(wrapper->path), "%s", path);
    return (hal_dir_t *)wrapper;
}

bool hal_readdir(hal_dir_t *directory, hal_dir_entry_t *entry) {
    if (!directory || !entry) return false;
    crowpanel_directory_t *wrapper = (crowpanel_directory_t *)directory;
    struct dirent *item = readdir(wrapper->directory);
    if (!item) return false;

    snprintf(entry->name, sizeof(entry->name), "%s", item->d_name);
    entry->is_dir = item->d_type == DT_DIR;
    entry->size = 0;

    if (item->d_type == DT_UNKNOWN || !entry->is_dir) {
        char full_path[MAX_PATH_LEN];
        int length = snprintf(full_path, sizeof(full_path), "%s/%s",
                              wrapper->path, item->d_name);
        if (length > 0 && (size_t)length < sizeof(full_path)) {
            struct stat status;
            if (stat(full_path, &status) == 0) {
                entry->is_dir = S_ISDIR(status.st_mode);
                if (status.st_size > 0) entry->size = (size_t)status.st_size;
            }
        }
    }
    return true;
}

void hal_closedir(hal_dir_t *directory) {
    if (!directory) return;
    crowpanel_directory_t *wrapper = (crowpanel_directory_t *)directory;
    if (wrapper->directory) closedir(wrapper->directory);
    free(wrapper);
}

#endif
