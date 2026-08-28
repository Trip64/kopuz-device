#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_audio.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// ==============================================================================
// Mikromedia Plus for STM32F7 VS1053B Audio Codec & Piezo Driver
// ==============================================================================
// Hardware Connections:
//   SPI2_SCK:  PB13 (AF5)
//   SPI2_MISO: PB14 (AF5)
//   SPI2_MOSI: PB15 (AF5)
//   MP3_CS:    PD11 (Control Chip Select, active low)
//   BSYNC/XDCS:PD10 (Data Chip Select, active low)
//   DREQ:      PD9  (Data Request, active high input)
//   MP3_RST:   PD8  (Hardware Reset, active low)
//   MMC_CS:    PD12 (Deselct SD on SPI2)
//   NRF_CS:    PG9  (Deselect nRF on SPI2)
//   PIEZO:     PB8  (Onboard Piezo Buzzer)

#define VS_SCK_PIN       GPIO_PIN_13
#define VS_SCK_PORT      GPIOB
#define VS_MISO_PIN      GPIO_PIN_14
#define VS_MISO_PORT     GPIOB
#define VS_MOSI_PIN      GPIO_PIN_15
#define VS_MOSI_PORT     GPIOB

#define VS_XCS_PIN       GPIO_PIN_11   /* MP3_CS */
#define VS_XCS_PORT      GPIOD
#define VS_XDCS_PIN      GPIO_PIN_10   /* BSYNC / XDCS */
#define VS_XDCS_PORT     GPIOD
#define VS_DREQ_PIN      GPIO_PIN_9    /* DREQ */
#define VS_DREQ_PORT     GPIOD
#define VS_RST_PIN       GPIO_PIN_8    /* MP3_RST */
#define VS_RST_PORT      GPIOD

#define MMC_CS_PIN       GPIO_PIN_12
#define MMC_CS_PORT      GPIOD
#define NRF_CS_PIN       GPIO_PIN_9
#define NRF_CS_PORT      GPIOG
#define BUZZER_PIN       GPIO_PIN_8
#define BUZZER_PORT      GPIOB

#define VS_XCS_HIGH()    (VS_XCS_PORT->BSRR = VS_XCS_PIN)
#define VS_XCS_LOW()     (VS_XCS_PORT->BSRR = (uint32_t)VS_XCS_PIN << 16)
#define VS_XDCS_HIGH()   (VS_XDCS_PORT->BSRR = VS_XDCS_PIN)
#define VS_XDCS_LOW()    (VS_XDCS_PORT->BSRR = (uint32_t)VS_XDCS_PIN << 16)
#define VS_RST_HIGH()    (VS_RST_PORT->BSRR = VS_RST_PIN)
#define VS_RST_LOW()     (VS_RST_PORT->BSRR = (uint32_t)VS_RST_PIN << 16)
#define VS_IS_DREQ()     ((VS_DREQ_PORT->IDR & VS_DREQ_PIN) != 0)

// VS1053 SCI Registers
#define SCI_MODE         0x00
#define SCI_STATUS       0x01
#define SCI_BASS         0x02
#define SCI_CLOCKF       0x03
#define SCI_DECODE_TIME  0x04
#define SCI_AUDATA       0x05
#define SCI_WRAM         0x06
#define SCI_WRAMADDR     0x07
#define SCI_HDAT0        0x08
#define SCI_HDAT1        0x09
#define SCI_AIADDR       0x0A
#define SCI_VOL          0x0B

static SPI_HandleTypeDef s_hspi2;
static uint8_t s_volume = 90;
static bool s_running = false;
static bool s_codec_ready = false;
static uint32_t s_sample_rate = 44100;
static uint8_t s_channels = 2;

static void vs1053_spi_slow(void) {
    s_hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; // ~800 kHz
    HAL_SPI_Init(&s_hspi2);
}

static void vs1053_spi_fast(void) {
    s_hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  // ~6.75 MHz
    HAL_SPI_Init(&s_hspi2);
}

static void vs1053_wait_dreq(void) {
    uint32_t timeout = 100000;
    while (!VS_IS_DREQ() && --timeout);
}

static void vs1053_sci_write(uint8_t reg, uint16_t val) {
    vs1053_wait_dreq();
    VS_XDCS_HIGH();
    VS_XCS_LOW();
    uint8_t cmd[4] = { 0x02, reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    HAL_SPI_Transmit(&s_hspi2, cmd, 4, 100);
    VS_XCS_HIGH();
    vs1053_wait_dreq();
}

static uint16_t vs1053_sci_read(uint8_t reg) {
    vs1053_wait_dreq();
    VS_XDCS_HIGH();
    VS_XCS_LOW();
    uint8_t cmd[2] = { 0x03, reg };
    uint8_t rx[2] = {0};
    HAL_SPI_Transmit(&s_hspi2, cmd, 2, 100);
    HAL_SPI_Receive(&s_hspi2, rx, 2, 100);
    VS_XCS_HIGH();
    vs1053_wait_dreq();
    return (rx[0] << 8) | rx[1];
}

static void vs1053_sine_test(uint8_t n, uint32_t duration_ms) {
    vs1053_sci_write(SCI_MODE, 0x0820); // SM_SDINEW | SM_TESTS
    vs1053_wait_dreq();

    uint8_t start_cmd[8] = { 0x53, 0xEF, 0x6E, n, 0x00, 0x00, 0x00, 0x00 };
    VS_XCS_HIGH();
    VS_XDCS_LOW();
    HAL_SPI_Transmit(&s_hspi2, start_cmd, 8, 100);
    VS_XDCS_HIGH();

    HAL_Delay(duration_ms);

    uint8_t stop_cmd[8] = { 0x45, 0x78, 0x69, 0x74, 0x00, 0x00, 0x00, 0x00 };
    VS_XCS_HIGH();
    VS_XDCS_LOW();
    HAL_SPI_Transmit(&s_hspi2, stop_cmd, 8, 100);
    VS_XDCS_HIGH();

    vs1053_sci_write(SCI_MODE, 0x0800);
}

static void vs1053_send_wav_header(uint32_t sample_rate, uint8_t channels) {
    uint8_t header[44];
    uint32_t byte_rate = sample_rate * channels * 2;
    uint16_t block_align = channels * 2;
    uint32_t data_size = 0x7FFFFFFF;
    uint32_t riff_size = data_size + 36;

    memcpy(&header[0], "RIFF", 4);
    header[4] = (uint8_t)(riff_size);
    header[5] = (uint8_t)(riff_size >> 8);
    header[6] = (uint8_t)(riff_size >> 16);
    header[7] = (uint8_t)(riff_size >> 24);
    memcpy(&header[8], "WAVEfmt ", 8);
    header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
    header[20] = 1; header[21] = 0;
    header[22] = channels; header[23] = 0;
    header[24] = (uint8_t)(sample_rate);
    header[25] = (uint8_t)(sample_rate >> 8);
    header[26] = (uint8_t)(sample_rate >> 16);
    header[27] = (uint8_t)(sample_rate >> 24);
    header[28] = (uint8_t)(byte_rate);
    header[29] = (uint8_t)(byte_rate >> 8);
    header[30] = (uint8_t)(byte_rate >> 16);
    header[31] = (uint8_t)(byte_rate >> 24);
    header[32] = (uint8_t)(block_align);
    header[33] = (uint8_t)(block_align >> 8);
    header[34] = 16; header[35] = 0;
    memcpy(&header[36], "data", 4);
    header[40] = (uint8_t)(data_size);
    header[41] = (uint8_t)(data_size >> 8);
    header[42] = (uint8_t)(data_size >> 16);
    header[43] = (uint8_t)(data_size >> 24);

    for (int i = 0; i < 44; i += 32) {
        int len = (44 - i > 32) ? 32 : (44 - i);
        vs1053_wait_dreq();
        VS_XCS_HIGH();
        VS_XDCS_LOW();
        HAL_SPI_Transmit(&s_hspi2, &header[i], len, 50);
        VS_XDCS_HIGH();
    }
}

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    s_sample_rate = sample_rate ? sample_rate : 44100;
    s_channels = channels ? channels : 2;

    // 1. Enable Clocks
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    // 2. Configure SPI2 GPIOs (PB13 SCK, PB14 MISO, PB15 MOSI)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = VS_SCK_PIN | VS_MISO_PIN | VS_MOSI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. Configure Control Pins (PD8 RST, PD10 BSYNC, PD11 MP3_CS, PD12 MMC_CS)
    GPIO_InitStruct.Pin = VS_XCS_PIN | VS_XDCS_PIN | VS_RST_PIN | MMC_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // Deselect MMC CS so it doesn't collide on SPI2
    HAL_GPIO_WritePin(MMC_CS_PORT, MMC_CS_PIN, GPIO_PIN_SET);

    // Deselect nRF CS on PG9
    GPIO_InitStruct.Pin = NRF_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(NRF_CS_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(NRF_CS_PORT, NRF_CS_PIN, GPIO_PIN_SET);

    // Piezo Buzzer on PB8
    GPIO_InitStruct.Pin = BUZZER_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    // PD9 DREQ input
    GPIO_InitStruct.Pin = VS_DREQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    VS_XCS_HIGH();
    VS_XDCS_HIGH();

    // 4. Hardware Reset (match mikroC MP3_Set_default_Mode)
    VS_RST_LOW();
    HAL_Delay(20);
    VS_RST_HIGH();
    HAL_Delay(20);

    // 5. Initialize SPI2 at slow speed
    s_hspi2.Instance = SPI2;
    s_hspi2.Init.Mode = SPI_MODE_MASTER;
    s_hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    s_hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    s_hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    s_hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    s_hspi2.Init.NSS = SPI_NSS_SOFT;
    s_hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    s_hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    s_hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    vs1053_spi_slow();

    vs1053_wait_dreq();

    // 6. Set default mode, clock, bass registers (matching mikroC example)
    vs1053_sci_write(SCI_MODE, 0x0800);   // SM_SDINEW (0x0800)
    vs1053_sci_write(SCI_BASS, 0x7A00);   // Bass/Treble boost
    vs1053_sci_write(SCI_CLOCKF, 0xC000); // 3.5x PLL for 12.288 MHz crystal
    HAL_Delay(5);

    // 7. Switch SPI to fast speed for audio data transfer
    vs1053_spi_fast();

    // 8. Set volume (0 = max, 254 = silence)
    hal_audio_set_volume(s_volume);

    // 9. Send standard WAV header for PCM decoding
    vs1053_send_wav_header(s_sample_rate, s_channels);

    s_codec_ready = true;
    s_running = true;
    return 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0 || !s_running || !s_codec_ready) return 0;

    uint8_t pcm_chunk[32];
    size_t chunk_idx = 0;
    size_t written = 0;

    for (size_t i = 0; i < sample_count; i++) {
        int32_t s = samples[i];
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        int16_t s16 = (int16_t)s;

        // Little Endian: Low byte then High byte
        pcm_chunk[chunk_idx++] = (uint8_t)(s16 & 0xFF);
        pcm_chunk[chunk_idx++] = (uint8_t)(s16 >> 8);

        if (chunk_idx >= 32) {
            vs1053_wait_dreq();
            VS_XCS_HIGH();
            VS_XDCS_LOW();
            HAL_SPI_Transmit(&s_hspi2, pcm_chunk, 32, 50);
            VS_XDCS_HIGH();
            chunk_idx = 0;
        }
        written++;
    }

    if (chunk_idx > 0) {
        vs1053_wait_dreq();
        VS_XCS_HIGH();
        VS_XDCS_LOW();
        HAL_SPI_Transmit(&s_hspi2, pcm_chunk, chunk_idx, 50);
        VS_XDCS_HIGH();
    }

    return written;
}

void hal_audio_set_volume(uint8_t volume) {
    s_volume = (volume > 100) ? 100 : volume;
    if (!s_codec_ready) return;

    // Convert 0..100% to VS1053 attenuation (0 = max, 254 = silence)
    // Map volume 100% -> 0x0000, 50% -> 0x2020, 0% -> 0xFEFE
    uint8_t atten;
    if (s_volume == 0) {
        atten = 0xFE;
    } else {
        atten = (uint8_t)((100 - s_volume) * 80 / 100); // 0 to 80 dB attenuation
    }
    uint16_t vol_reg = ((uint16_t)atten << 8) | atten;
    vs1053_sci_write(SCI_VOL, vol_reg);
}

void hal_audio_stop(void) {
    s_running = false;
}

void hal_audio_resume(void) {
    s_running = true;
}

bool hal_audio_needs_data(void) {
    return s_running && s_codec_ready && VS_IS_DREQ();
}

void hal_audio_close(void) {
    s_running = false;
}

void hal_audio_beep(uint16_t freq_hz, uint16_t duration_ms) {
    if (freq_hz == 0) freq_hz = 1500;
    if (duration_ms == 0) duration_ms = 25;

    // 1. Toggle on-board Piezo Buzzer (PB8) for physical tactile click
    uint32_t half_period_us = 1000000 / (freq_hz * 2);
    uint32_t cycles = (freq_hz * duration_ms) / 1000;
    for (uint32_t i = 0; i < cycles; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        for (volatile int d = 0; d < 400; d++);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        for (volatile int d = 0; d < 400; d++);
    }

    // 2. Play VS1053 Hardware Sine Tone on Earphones
    if (s_codec_ready) {
        // VS1053 Sine test Freq parameter: 0x7E = ~1000Hz @ 44.1kHz
        vs1053_sine_test(0x7E, duration_ms);
    }
}

#endif
