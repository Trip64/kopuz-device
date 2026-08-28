#if defined(NRF52840_XXAA) || defined(TARGET_NRF52840)

#include "hal/hal_system.h"
#include <stdint.h>

static uint32_t s_tick_ms = 0;

uint32_t hal_get_time_ms(void) {
    return s_tick_ms++;
}

void hal_delay_ms(uint32_t ms) {
    s_tick_ms += ms;
}

uint32_t hal_system_get_ram_used_bytes(void) {
    return 48 * 1024;
}

uint32_t hal_random(void) {
    static uint32_t s_lfsr = 0xACE1u;
    s_lfsr = (s_lfsr >> 1) ^ (-(s_lfsr & 1u) & 0xB400u);
    return s_lfsr;
}

void hal_system_reboot(void) {
    // NVIC_SystemReset();
}

#endif
