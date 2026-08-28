#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

void NMI_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)      __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
};

static void SystemInit(void) {
    // 1. Enable FPU (Coprocessor CP10 & CP11 Full Access)
    #define SCB_CPACR (*((volatile uint32_t*)0xE000ED88))
    SCB_CPACR |= ((3UL << 10*2) | (3UL << 11*2));

    // 2. Enable AHB1 GPIO Clocks (GPIOA..GPIOG)
    #define RCC_AHB1ENR (*((volatile uint32_t*)0x40023830))
    RCC_AHB1ENR |= 0x7F;

    // 3. Configure Status RGB LEDs (PG15 = Red, PB3 = Green, PB4 = Blue)
    #define GPIOB_MODER (*((volatile uint32_t*)0x40020400))
    #define GPIOB_BSRR  (*((volatile uint32_t*)0x40020418))
    #define GPIOG_MODER (*((volatile uint32_t*)0x40021800))
    #define GPIOG_BSRR  (*((volatile uint32_t*)0x40021818))

    GPIOB_MODER = (GPIOB_MODER & ~0x000003C0) | 0x00000140; // Output on PB3, PB4
    GPIOG_MODER = (GPIOG_MODER & ~0xC0000000) | 0x40000000; // Output on PG15

    // Turn ON Green & Blue Status LEDs on board
    GPIOB_BSRR = (1UL << 3) | (1UL << 4);
}

void Reset_Handler(void) {
    // Hardware system init
    SystemInit();

    // Copy .data section from Flash to RAM
    uint32_t *pSrc = &_sidata;
    uint32_t *pDst = &_sdata;
    while (pDst < &_edata) {
        *pDst++ = *pSrc++;
    }

    // Zero fill the .bss section
    pDst = &_sbss;
    while (pDst < &_ebss) {
        *pDst++ = 0;
    }

    // Run application
    main();

    while (1) {
        __asm volatile("wfi");
    }
}

void Default_Handler(void) {
    while (1) {
        __asm volatile("nop");
    }
}
