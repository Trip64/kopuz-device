#include "hal/hal_system.h"
#include <SDL.h>
#include <stdlib.h>
#include <time.h>

void hal_delay_ms(uint32_t ms) {
    SDL_Delay(ms);
}

uint32_t hal_get_time_ms(void) {
    return (uint32_t)SDL_GetTicks();
}

uint64_t hal_get_time_us(void) {
    return (uint64_t)SDL_GetPerformanceCounter() * 1000000ULL / (uint64_t)SDL_GetPerformanceFrequency();
}

uint32_t hal_random(void) {
    return (uint32_t)rand();
}

hal_mutex_t hal_mutex_create(void) {
    return (hal_mutex_t)SDL_CreateMutex();
}

void hal_mutex_lock(hal_mutex_t m) {
    if (m) SDL_LockMutex((SDL_mutex*)m);
}

void hal_mutex_unlock(hal_mutex_t m) {
    if (m) SDL_UnlockMutex((SDL_mutex*)m);
}

void hal_mutex_destroy(hal_mutex_t m) {
    if (m) SDL_DestroyMutex((SDL_mutex*)m);
}

static int thread_trampoline(void *data) {
    hal_thread_fn_t fn = (hal_thread_fn_t)data;
    fn(NULL);
    return 0;
}

bool hal_thread_create(const char *name, hal_thread_fn_t fn, void *arg, uint32_t stack_size, int priority) {
    (void)stack_size; (void)priority; (void)arg;
    SDL_Thread *th = SDL_CreateThread(thread_trampoline, name, (void*)fn);
    return (th != NULL);
}

uint32_t hal_system_get_ram_used_bytes(void) {
    return 0;
}

void hal_system_reboot(void) {
    exit(0);
}
