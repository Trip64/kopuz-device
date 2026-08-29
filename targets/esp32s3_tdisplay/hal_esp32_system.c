#if defined(ESP_PLATFORM)

#include "hal/hal_system.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

uint32_t hal_get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void hal_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

hal_mutex_t hal_mutex_create(void) {
    return (hal_mutex_t)xSemaphoreCreateMutex();
}

void hal_mutex_lock(hal_mutex_t mutex) {
    if (mutex) {
        xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY);
    }
}

void hal_mutex_unlock(hal_mutex_t mutex) {
    if (mutex) {
        xSemaphoreGive((SemaphoreHandle_t)mutex);
    }
}

void hal_mutex_destroy(hal_mutex_t mutex) {
    if (mutex) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex);
    }
}

uint32_t hal_random(void) {
    return esp_random();
}

uint32_t hal_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (esp_random() % (max - min + 1));
}

#include "esp_heap_caps.h"

uint32_t hal_system_get_ram_used_bytes(void) {
    size_t total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    size_t free_sz = esp_get_free_heap_size();
    return (total > free_sz) ? (uint32_t)(total - free_sz) : 0;
}

#endif
