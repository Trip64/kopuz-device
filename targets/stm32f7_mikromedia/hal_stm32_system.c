#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_system.h"
#include <stdint.h>

#define DEMCR           (*((volatile uint32_t*)0xE000EDFC))
#define DWT_CTRL        (*((volatile uint32_t*)0xE0001000))
#define DWT_CYCCNT      (*((volatile uint32_t*)0xE0001004))

// STM32F746 running on 16 MHz HSI default or 216 MHz HSE
#define CPU_FREQ_HZ     216000000UL

static bool s_dwt_init = false;

static void dwt_init_if_needed(void) {
    if (!s_dwt_init) {
        DEMCR |= (1UL << 24);   // Enable DWT
        DWT_CTRL |= (1UL << 0); // Enable Cycle Counter
        s_dwt_init = true;
    }
}

uint32_t hal_get_time_ms(void) {
    dwt_init_if_needed();
    // Milliseconds = Cycles / (CPU_FREQ_HZ / 1000)
    return (uint32_t)(DWT_CYCCNT / (CPU_FREQ_HZ / 1000UL));
}

void hal_delay_ms(uint32_t ms) {
    dwt_init_if_needed();
    uint32_t start = DWT_CYCCNT;
    uint32_t cycles = ms * (CPU_FREQ_HZ / 1000UL);
    while ((DWT_CYCCNT - start) < cycles) {
        __asm volatile("nop");
    }
}

uint32_t hal_system_get_ram_used_bytes(void) {
    return 64 * 1024;
}

uint32_t hal_random(void) {
    dwt_init_if_needed();
    static uint32_t s_lfsr = 0x5D43u;
    s_lfsr = (s_lfsr >> 1) ^ (-(s_lfsr & 1u) & 0xB400u) ^ DWT_CYCCNT;
    return s_lfsr;
}

void hal_system_reboot(void) {
    #define SCB_AIRCR (*((volatile uint32_t*)0xE000ED0C))
    SCB_AIRCR = 0x05FA0004; // System Reset Request
    while (1);
}

void hal_system_enter_dfu(void) {
    hal_system_reboot();
}

#endif
