#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_system.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>

#define DFU_BOOTLOADER_ADDR   0x1FFF0000
#define DFU_MAGIC_VAL         0xDEADBEEF
#define DFU_FLAG_PTR          ((volatile uint32_t *)0x2000FFF0)

static uint8_t s_mutex_token;

void hal_system_init(void) {
}

uint32_t hal_get_time_ms(void) {
    return HAL_GetTick();
}

uint64_t hal_get_time_us(void) {
    return (uint64_t)HAL_GetTick() * 1000u;
}

void hal_delay_ms(uint32_t ms) {
    HAL_Delay(ms);
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

uint32_t hal_system_get_tick(void) {
    return HAL_GetTick();
}

void hal_system_delay(uint32_t ms) {
    HAL_Delay(ms);
}

void hal_system_reset(void) {
    NVIC_SystemReset();
}

void hal_system_reboot(void) {
    NVIC_SystemReset();
}

void hal_system_enter_dfu(void) {
    *DFU_FLAG_PTR = DFU_MAGIC_VAL;
    NVIC_SystemReset();
}

void hal_system_check_dfu(void) {
    if (*DFU_FLAG_PTR == DFU_MAGIC_VAL) {
        *DFU_FLAG_PTR = 0;

        __disable_irq();

        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        // Reset clocks to internal HSI
        RCC->CR |= RCC_CR_HSION;
        while (!(RCC->CR & RCC_CR_HSIRDY));
        RCC->CFGR = 0x00000000;
        while ((RCC->CFGR & RCC_CFGR_SWS) != 0);

        // Remap System Flash Memory (0x1FFF0000) to 0x00000000
        __HAL_RCC_SYSCFG_CLK_ENABLE();
        SYSCFG->MEMRMP = 0x01;

        uint32_t app_stack = *(volatile uint32_t *)DFU_BOOTLOADER_ADDR;
        void (*bootloader_entry)(void) = (void (*)(void))(*(volatile uint32_t *)(DFU_BOOTLOADER_ADDR + 4));

        __set_MSP(app_stack);
        bootloader_entry();
        while (1);
    }
}

uint32_t hal_system_get_ram_used_bytes(void) {
    extern char _end;
    extern char _sdata;
    return (uint32_t)(&_end - &_sdata);
}

uint32_t hal_random(void) {
    return HAL_GetTick() * 1103515245 + 12345;
}

#endif
