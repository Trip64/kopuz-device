#include "hal/hal_audio.h"

#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "audio_i2s.pio.h"

#ifndef PICO_I2S_DATA_PIN
#define PICO_I2S_DATA_PIN        9
#endif
#ifndef PICO_I2S_CLOCK_PIN_BASE
#define PICO_I2S_CLOCK_PIN_BASE  10 // 10 = BCLK, 11 = LRCLK
#endif

#define DMA_BUFFER_SAMPLES       512
static int32_t s_dma_buf[2][DMA_BUFFER_SAMPLES];
static volatile int s_active_buf = 0;
static int s_dma_chan = -1;
static PIO s_pio = pio0;
static uint s_sm = 0;
static uint8_t s_volume = 70;

static void dma_handler(void) {
    dma_hw->ints0 = 1u << s_dma_chan;
    s_active_buf ^= 1;
    dma_channel_set_read_addr(s_dma_chan, s_dma_buf[s_active_buf], true);
}

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    (void)channels;
    sample_rate = sample_rate ? sample_rate : 44100;

    uint offset = pio_add_program(s_pio, &audio_i2s_program);
    pio_sm_config c = audio_i2s_program_get_default_config(offset);

    sm_config_set_out_pins(&c, PICO_I2S_DATA_PIN, 1);
    sm_config_set_sideset_pins(&c, PICO_I2S_CLOCK_PIN_BASE);
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // 32 bits per frame * 2 channels = 64 clocks per sample
    float div = (float)clock_get_hz(clk_sys) / (float)(sample_rate * 64 * 2);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(s_pio, s_sm, offset, &c);
    pio_sm_set_enabled(s_pio, s_sm, true);

    if (s_dma_chan < 0) {
        s_dma_chan = dma_claim_unused_channel(true);
        dma_channel_config dc = dma_channel_get_default_config(s_dma_chan);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
        channel_config_set_read_increment(&dc, true);
        channel_config_set_write_increment(&dc, false);
        channel_config_set_dreq(&dc, pio_get_dreq(s_pio, s_sm, true));

        dma_channel_configure(
            s_dma_chan,
            &dc,
            &s_pio->txf[s_sm],
            s_dma_buf[0],
            DMA_BUFFER_SAMPLES,
            false
        );

        dma_channel_set_irq0_enabled(s_dma_chan, true);
        irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }

    return 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0) return 0;

    int inactive = s_active_buf ^ 1;
    size_t count = sample_count > DMA_BUFFER_SAMPLES ? DMA_BUFFER_SAMPLES : sample_count;

    for (size_t i = 0; i < count; i++) {
        int64_t s = ((int64_t)samples[i] * s_volume) / 100;
        s_dma_buf[inactive][i] = (int32_t)s;
    }

    if (!dma_channel_is_busy(s_dma_chan)) {
        dma_channel_start(s_dma_chan);
    }
    return count;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = volume > 100 ? 100 : volume;
}

void hal_audio_stop(void) {
    if (s_dma_chan >= 0) dma_channel_abort(s_dma_chan);
}

void hal_audio_resume(void) {
    if (s_dma_chan >= 0 && !dma_channel_is_busy(s_dma_chan)) {
        dma_channel_start(s_dma_chan);
    }
}

void hal_audio_close(void) {
    hal_audio_stop();
}

bool hal_audio_needs_data(void) {
    return true;
}

#endif
