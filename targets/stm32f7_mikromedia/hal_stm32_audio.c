#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "hal/hal_audio.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// ==============================================================================
// Mikromedia Plus for STM32F7 VS1053B Audio Codec Pinout (SPI2)
// ==============================================================================
#define VS_SCK_PIN       GPIO_PIN_13
#define VS_SCK_PORT      GPIOB
#define VS_MISO_PIN      GPIO_PIN_14
#define VS_MISO_PORT     GPIOB
#define VS_MOSI_PIN      GPIO_PIN_15
#define VS_MOSI_PORT     GPIOB

#define VS_XCS_PIN       GPIO_PIN_11   /* Control Chip Select */
#define VS_XCS_PORT      GPIOD
#define VS_XDCS_PIN      GPIO_PIN_10   /* Data Chip Select */
#define VS_XDCS_PORT     GPIOD
#define VS_DREQ_PIN      GPIO_PIN_9    /* Data Request Input */
#define VS_DREQ_PORT     GPIOD
#define VS_RST_PIN       GPIO_PIN_8    /* Hardware Reset */
#define VS_RST_PORT      GPIOD

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
static uint8_t s_volume = 70;
static bool s_running = false;
static bool s_codec_ready = false;

static void vs1053_spi_slow(void) {
    s_hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; // ~800 kHz
    HAL_SPI_Init(&s_hspi2);
}

static void vs1053_spi_fast(void) {
    s_hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  // ~6.75 MHz
    HAL_SPI_Init(&s_hspi2);
}

static void vs1053_wait_dreq(void) {
    uint32_t timeout = 50000;
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

int hal_audio_init(uint32_t sample_rate, uint8_t channels) {
    (void)sample_rate;
    (void)channels;

    // 1. Enable Clocks
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    // 2. Configure SPI2 GPIOs
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = VS_SCK_PIN | VS_MISO_PIN | VS_MOSI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. Configure Control Pins
    GPIO_InitStruct.Pin = VS_XCS_PIN | VS_XDCS_PIN | VS_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = VS_DREQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    VS_XCS_HIGH();
    VS_XDCS_HIGH();

    // 4. Hardware Reset
    VS_RST_LOW();
    HAL_Delay(10);
    VS_RST_HIGH();
    HAL_Delay(10);

    // 5. Initialize SPI2
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

    // 6. Configure VS1053B clock (3.5x PLL for 12.288 MHz crystal -> 43 MHz core)
    vs1053_sci_write(SCI_CLOCKF, 0x8800);
    HAL_Delay(2);
    vs1053_spi_fast();

    // 7. Set initial volume (0x0000 = max, 0xFEFE = min)
    hal_audio_set_volume(s_volume);

    s_codec_ready = true;
    s_running = true;
    return 0;
}

size_t hal_audio_write(const int32_t *samples, size_t sample_count) {
    if (!samples || sample_count == 0 || !s_running || !s_codec_ready) return 0;

    // Convert 32-bit PCM to 16-bit stereo PCM and send in 32-byte chunks
    uint8_t pcm_chunk[32];
    size_t chunk_idx = 0;
    size_t written = 0;

    for (size_t i = 0; i < sample_count; i++) {
        int32_t s = samples[i];
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        int16_t s16 = (int16_t)s;

        pcm_chunk[chunk_idx++] = (uint8_t)(s16 >> 8);
        pcm_chunk[chunk_idx++] = (uint8_t)(s16 & 0xFF);

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
    uint8_t atten = (uint8_t)((100 - s_volume) * 254 / 100);
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

#endif
