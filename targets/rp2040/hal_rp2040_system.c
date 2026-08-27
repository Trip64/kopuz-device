#include "hal/hal_system.h"

#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "hardware/timer.h"
#include <stdlib.h>

uint32_t hal_get_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void hal_delay_ms(uint32_t ms) {
    sleep_ms(ms);
}

hal_mutex_t hal_mutex_create(void) {
    mutex_t *m = (mutex_t*)malloc(sizeof(mutex_t));
    if (m) {
        mutex_init(m);
    }
    return (hal_mutex_t)m;
}

void hal_mutex_lock(hal_mutex_t mutex) {
    if (mutex) {
        mutex_enter_blocking((mutex_t*)mutex);
    }
}

void hal_mutex_unlock(hal_mutex_t mutex) {
    if (mutex) {
        mutex_exit((mutex_t*)mutex);
    }
}

void hal_mutex_destroy(hal_mutex_t mutex) {
    if (mutex) {
        free(mutex);
    }
}

uint32_t hal_random(void) {
    static uint32_t s_seed = 0;
    if (s_seed == 0) s_seed = time_us_32();
    s_seed = s_seed * 1664525u + 1013904223u; // Linear congruential generator
    return s_seed;
}

uint32_t hal_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (hal_random() % (max - min + 1));
}

uint32_t hal_system_get_ram_used_bytes(void) {
    extern char __bss_start__[];
    extern char __bss_end__[];
    return (uint32_t)(__bss_end__ - __bss_start__);
}

#endif
