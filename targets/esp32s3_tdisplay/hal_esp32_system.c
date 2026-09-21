#if defined(ESP_PLATFORM)

#include "hal/hal_system.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdlib.h>

typedef struct {
    hal_thread_fn_t fn;
    void *arg;
} esp_thread_context_t;

static void thread_trampoline(void *data) {
    esp_thread_context_t *context = (esp_thread_context_t*)data;
    hal_thread_fn_t fn = context->fn;
    void *arg = context->arg;
    free(context);
    fn(arg);
    vTaskDelete(NULL);
}

uint32_t hal_get_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

uint64_t hal_get_time_us(void) {
    return (uint64_t)esp_timer_get_time();
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

uint32_t hal_system_random(void) {
    return esp_random();
}

bool hal_thread_create(const char *name, hal_thread_fn_t fn, void *arg,
                       uint32_t stack_size, int priority) {
    if (!fn) return false;
    esp_thread_context_t *context = (esp_thread_context_t*)malloc(sizeof(*context));
    if (!context) return false;
    context->fn = fn;
    context->arg = arg;

    uint32_t task_stack_size = stack_size < 4096 ? 4096 : stack_size;
    UBaseType_t task_priority = priority > 0 ? (UBaseType_t)priority : tskIDLE_PRIORITY + 1;
    if (xTaskCreate(thread_trampoline, name ? name : "kopuz", task_stack_size, context,
                    task_priority, NULL) != pdPASS) {
        free(context);
        return false;
    }
    return true;
}

uint32_t hal_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (esp_random() % (max - min + 1));
}

#include "esp_heap_caps.h"

#include "esp_system.h"

uint32_t hal_system_get_ram_used_bytes(void) {
    size_t total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    size_t free_sz = esp_get_free_heap_size();
    return (total > free_sz) ? (uint32_t)(total - free_sz) : 0;
}

void hal_system_reboot(void) {
    esp_restart();
}

#endif
