# Nordic Semiconductor nRF54L15 Port Architecture

The nRF54L15 is Nordic Semiconductor's next-generation ultra-low-power wireless SoC, featuring:
- ARM Cortex-M33 application core running at 128 MHz (with FPU and TrustZone)
- RISC-V coprocessor for background event processing
- 1.52 MB Non-Volatile RRAM (NVMC) and 256 KB SRAM
- Active power consumption ~30 uA/MHz (sub-microamp sleep < 0.8 uA)
- Hardware Serial Audio Interface (SAI / I2S)
- Native Bluetooth 5.4 LE Audio (LC3 Auracast / CIS / BIS)

---

## 1. Hardware Pinout & Wiring

| Interface | Peripheral Pin | Function | External Connection |
|:---|:---|:---|:---|
| **I2S DAC** | P1.04 | I2S SCK / BCLK | Bit Clock (e.g. PCM5102A / MAX98357A) |
| **I2S DAC** | P1.05 | I2S LRCK / WS | Word Select / LR Clock |
| **I2S DAC** | P1.06 | I2S SDOUT | Serial Audio Data |
| **MicroSD SPI** | P2.00 | SPIM CLK | MicroSD SCK |
| **MicroSD SPI** | P2.01 | SPIM MOSI | MicroSD MOSI / CMD |
| **MicroSD SPI** | P2.02 | SPIM MISO | MicroSD MISO / DAT0 |
| **MicroSD SPI** | P2.03 | GPIO CS | MicroSD Chip Select |
| **Display (I2C OLED)** | P0.04 | TWIM SCL | SSD1306 SCL |
| **Display (I2C OLED)** | P0.05 | TWIM SDA | SSD1306 SDA |
| **Display (SPI LCD)** | P0.08 | SPIM SCK | ST7789 SCL |
| **Display (SPI LCD)** | P0.09 | SPIM MOSI | ST7789 SDA |
| **Display (SPI LCD)** | P0.10 | GPIO DC | ST7789 Data/Command |
| **Display (SPI LCD)** | P0.11 | GPIO CS | ST7789 Chip Select |
| **Navigation Buttons** | P0.14..18 | GPIOTE | Enter, Up, Down, Back, Next |
| **Battery ADC** | P0.01 / AIN0 | SAADC | 1/6 Gain LiPo Battery Sensing |

---

## 2. Audio Architecture

### A. Wired Hardware I2S Output
- Uses the Global Serial Audio Interface (SAI) peripheral.
- Configured in Master mode with 32-bit slot width, feeding stereo 44.1 kHz / 48 kHz PCM samples via high-speed DMA buffers.

### B. [EXPERIMENTAL] Bluetooth 5.4 LE Audio (Auracast)
- Encodes 10ms PCM chunks into LC3 audio frames (32 kbps to 96 kbps per channel).
- Broadcasts audio over a Broadcast Isochronous Stream (BIS) using standard Basic Audio Profile (BAP) announcement UUIDs.
- Enables direct wireless streaming to modern LE Audio earphones.
