#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

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
    return 64 * 1024;
}

uint32_t hal_random(void) {
    static uint32_t s_lfsr = 0x5D43u;
    s_lfsr = (s_lfsr >> 1) ^ (-(s_lfsr & 1u) & 0xB400u);
    return s_lfsr;
}

void hal_system_reboot(void) {
    // NVIC_SystemReset();
}

// In-Application Software Jump to STM32F7 Factory System Memory DFU Bootloader
void hal_system_enter_dfu(void) {
    // STM32F746 System Memory bootloader base address: 0x1FF00000
    #define STM32F7_SYSTEM_MEMORY_ADDR 0x1FF00000
    typedef void (*dfu_boot_func_t)(void);

    // Disable all interrupts and reset SysTick
    // uint32_t *boot_vector = (uint32_t*)STM32F7_SYSTEM_MEMORY_ADDR;
    // __set_MSP(boot_vector[0]);
    // dfu_boot_func_t jump_to_boot = (dfu_boot_func_t)boot_vector[1];
    // jump_to_boot();
}

#endif
