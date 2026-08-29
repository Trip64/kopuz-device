#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void hal_delay_ms(uint32_t ms);
uint32_t hal_get_time_ms(void);
uint64_t hal_get_time_us(void);
uint32_t hal_random(void);

typedef void* hal_mutex_t;
hal_mutex_t hal_mutex_create(void);
void hal_mutex_lock(hal_mutex_t m);
void hal_mutex_unlock(hal_mutex_t m);
void hal_mutex_destroy(hal_mutex_t m);

typedef void (*hal_thread_fn_t)(void *arg);
bool hal_thread_create(const char *name, hal_thread_fn_t fn, void *arg, uint32_t stack_size, int priority);

uint32_t hal_system_get_ram_used_bytes(void);
void hal_system_reboot(void);

#ifdef __cplusplus
}
#endif
