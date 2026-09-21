#if defined(NRF52840_XXAA) || defined(TARGET_NRF52840)

#include "hal/hal_system.h"
#include <stdint.h>

static uint32_t s_tick_ms = 0;
static uint8_t s_mutex_token;

uint32_t hal_get_time_ms(void) {
    return s_tick_ms++;
}

uint64_t hal_get_time_us(void) {
    return (uint64_t)s_tick_ms * 1000u;
}

void hal_delay_ms(uint32_t ms) {
    s_tick_ms += ms;
}

uint32_t hal_system_get_ram_used_bytes(void) {
    return 48 * 1024;
}

uint32_t hal_system_random(void) {
    static uint32_t s_lfsr = 0xACE1u;
    s_lfsr = (s_lfsr >> 1) ^ (-(s_lfsr & 1u) & 0xB400u);
    return s_lfsr;
}

hal_mutex_t hal_mutex_create(void) { return &s_mutex_token; }
void hal_mutex_lock(hal_mutex_t mutex) { (void)mutex; }
void hal_mutex_unlock(hal_mutex_t mutex) { (void)mutex; }
void hal_mutex_destroy(hal_mutex_t mutex) { (void)mutex; }

bool hal_thread_create(const char *name, hal_thread_fn_t fn, void *arg,
                       uint32_t stack_size, int priority) {
    (void)name;
    (void)fn;
    (void)arg;
    (void)stack_size;
    (void)priority;
    return false;
}

void hal_system_reboot(void) {
    // NVIC_SystemReset();
}

#endif
