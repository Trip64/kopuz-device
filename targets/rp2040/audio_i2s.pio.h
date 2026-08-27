// Generated/standard 16/32-bit I2S PIO program for RP2040 / RP2350
#pragma once

#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include "hardware/pio.h"

static const uint16_t audio_i2s_instructions[] = {
    0x7001, //  0: out    pins, 1         side 2
    0x1940, //  1: jmp    x--, 0          side 3
    0x6001, //  2: out    pins, 1         side 0
    0xe82e, //  3: set    x, 14           side 1
    0x6001, //  4: out    pins, 1         side 0
    0x0944, //  5: jmp    x--, 4          side 1
    0x7001, //  6: out    pins, 1         side 2
    0xf82e  //  7: set    x, 14           side 3
};

static const struct pio_program audio_i2s_program = {
    .instructions = audio_i2s_instructions,
    .length = 8,
    .origin = -1,
};

static inline pio_sm_config audio_i2s_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 7);
    sm_config_set_sideset(&c, 2, false, false);
    return c;
}
#endif
