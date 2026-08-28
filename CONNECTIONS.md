# Hardware Connections and Pinout Guide

This document specifies the wiring and pin connections for the Kopuz Device C/C++ firmware across supported hardware targets:

1. Raspberry Pi Pico (RP2040) and Pico 2 (RP2350) with original ILI9341 320x240 Display
2. LilyGO T-Display S3 (ESP32-S3) with onboard 1.9-inch ST7789 320x170 Display

---

## 1. Raspberry Pi Pico (RP2040) and Pico 2 (RP2350)

The RP2040 / RP2350 firmware runs at 320x240 using the original ILI9341 SPI TFT LCD from the original repository.

### 1.1 Original Display Wiring (ILI9341 320x240 SPI)
Connected to hardware SPI0:

| ILI9341 Pin | Signal Name | Pico Pin | Pico Physical Pin | Notes |
|:---|:---|:---|:---|:---|
| VCC | Power | 3V3 (OUT) | Pin 36 | 3.3V power |
| GND | Ground | GND | Pin 38 / 3 | Ground |
| CS | Chip Select | GP17 | Pin 22 | SPI0 CSn (Active-low) |
| RESET | Hardware Reset | GP21 | Pin 27 | Active-low reset pulse |
| DC / A0 | Data / Command | GP20 | Pin 26 | Low = Command, High = Data |
| SDI (MOSI) | SPI Data In | GP19 | Pin 25 | SPI0 TX |
| SCK | SPI Clock | GP18 | Pin 24 | SPI0 SCK (40 MHz) |
| LED / BL | Backlight | GP22 | Pin 29 | Hardware PWM brightness control |
| SDO (MISO) | SPI Data Out | GP16 | Pin 21 | Optional (display is write-only) |

---

### 1.2 Audio Output (I2S DAC: PCM5102A / MAX98357A)
Driven by the RP2040 / RP2350 Programmable I/O (PIO) engine with DMA ping-pong buffers:

| DAC Pin | Signal Name | Pico Pin | Pico Physical Pin | Notes |
|:---|:---|:---|:---|:---|
| VIN / VCC | Power | VBUS (5V) / 3V3 | Pin 40 / 36 | 5V for PCM5102 / MAX98357 |
| GND | Ground | GND | Pin 13 / 18 | Ground |
| LCK / WS | Word Select (LRCLK) | GP11 | Pin 15 | Audio frame clock (44.1 kHz) |
| BCK | Bit Clock (BCLK) | GP10 | Pin 14 | 32-bit/sample clock |
| DIN / DATA | Serial Audio Data | GP9 | Pin 12 | I2S serialized PCM |
| SCK | System Clock | GND | Pin 13 | Connect to GND (DAC uses internal PLL) |

---

### 1.3 MicroSD Card Reader (SPI1)
Connected to dedicated SPI1 bus to prevent bus contention with the display:

| MicroSD Pin | Signal Name | Pico Pin | Pico Physical Pin | Notes |
|:---|:---|:---|:---|:---|
| VCC | Power | 3V3 (OUT) | Pin 36 | 3.3V |
| GND | Ground | GND | Pin 23 | Ground |
| CS | Chip Select | GP13 | Pin 17 | SPI1 CSn |
| MOSI / DI | Master Out Slave In | GP15 | Pin 20 | SPI1 TX |
| SCK / CLK | SPI Clock | GP14 | Pin 19 | SPI1 SCK |
| MISO / DO | Master In Slave Out | GP12 | Pin 16 | SPI1 RX |

---

### 1.4 Navigation Buttons (Active-Low)
All pushbuttons connect between the GPIO pin and GND. Internal pull-up resistors are enabled in software.

| Button | Function | Pico Pin | Pico Physical Pin | Behavior |
|:---|:---|:---|:---|:---|
| BTN 1 | Play / Pause / Select | GP2 | Pin 4 | Short press: Select / Toggle Play. Long press (>= 500 ms): BACK |
| BTN 2 | Next / Scroll Down | GP3 | Pin 5 | Skip track or scroll list down |
| BTN 3 | Prev / Scroll Up | GP4 | Pin 6 | Previous track or scroll list up |
| BTN 4 | Volume Up | GP5 | Pin 7 | Increase volume |
| BTN 5 | Volume Down | GP6 | Pin 9 | Decrease volume |

---

### 1.5 Battery Voltage Sensing
Uses the onboard RP2040 12-bit ADC0:

```
Battery (+) ----[ 100 kohm ]----+----> GP26 (ADC0 / Pin 31)
                                |
                             [ 100 kohm ]
                                |
                               GND
```
- Voltage Scaling: Vbatt = ADC * (3.3V * 2 / 4095) = ADC * 6600 / 4095 mV.

---

## 2. LilyGO T-Display S3 (ESP32-S3)

The LilyGO T-Display S3 uses its own onboard 1.9-inch ST7789V LCD panel operating in landscape mode at 320x170 pixels.

### 2.1 Onboard 8-bit Parallel Display (ST7789V 320x170)
Hardwired internally on the LilyGO PCB:

| Signal | ESP32-S3 GPIO | Description |
|:---|:---|:---|
| LCD_CS | GPIO 6 | Chip Select |
| LCD_DC | GPIO 7 | Data / Command |
| LCD_WR | GPIO 8 | Write strobe clock |
| LCD_RD | GPIO 9 | Read strobe (held High) |
| LCD_RST | GPIO 5 | Hardware Reset |
| LCD_BL | GPIO 38 | Backlight control |
| LCD_PWR | GPIO 15 | Main power rail switch (set High to power display) |
| LCD_D0 | GPIO 39 | Data bit 0 |
| LCD_D1 | GPIO 40 | Data bit 1 |
| LCD_D2 | GPIO 41 | Data bit 2 |
| LCD_D3 | GPIO 42 | Data bit 3 |
| LCD_D4 | GPIO 45 | Data bit 4 |
| LCD_D5 | GPIO 46 | Data bit 5 |
| LCD_D6 | GPIO 47 | Data bit 6 |
| LCD_D7 | GPIO 48 | Data bit 7 |

---

### 2.2 External I2S DAC (PCM5102A / MAX98357A)
Connect external I2S DAC module to the header pins:

| DAC Pin | Signal Name | ESP32-S3 GPIO | Notes |
|:---|:---|:---|:---|
| VCC | Power | 5V / 3V3 | Power rail |
| GND | Ground | GND | Common ground |
| BCK | Bit Clock (BCLK) | GPIO 43 | Serial clock |
| WS / LCK | Word Select (LRCLK) | GPIO 44 | Frame clock |
| DOUT / DIN | Audio Data Out | GPIO 1 | Serialized PCM data |

---

### 2.3 MicroSD Card Reader (SPI)
Connect external MicroSD card adapter to the header pins:

| MicroSD Pin | ESP32-S3 GPIO | Notes |
|:---|:---|:---|
| VCC | 3V3 | Power |
| GND | GND | Ground |
| CS | GPIO 10 | Card Select |
| MOSI | GPIO 11 | Data to card |
| CLK | GPIO 12 | SPI Clock |
| MISO | GPIO 13 | Data from card |

---

### 2.4 Button Inputs
- Onboard Button 1 (Boot): GPIO 0 (Play/Pause / Select. Hold >= 500 ms for Back).
- Onboard Button 2: GPIO 14 (Next / Scroll).
- External Button Vol+ (Optional): GPIO 2.
- External Button Vol- (Optional): GPIO 3.
- External Button Prev (Optional): GPIO 16.

### 2.5 Battery Voltage Sensing
- Onboard ADC: GPIO 4 (ADC1 Channel 3) is connected internally to an onboard 100 kohm / 100 kohm voltage divider reading the battery terminal.

---

## 3. Nordic Semiconductor nRF52840

The nRF52840 firmware supports both high-quality I2S audio output and multiple display configurations (I2C OLED, SPI Color LCD, or E-Ink).

### 3.1 Audio Output (I2S Master + EasyDMA)
Connects to standard 3-wire I2S DACs (e.g. PCM5102A, MAX98357A, UDA1334A):

| DAC Pin | Signal Name | nRF52840 Pin | Notes |
|:---|:---|:---|:---|
| VIN / VCC | Power | 3.3V / 5V | Power for DAC module |
| GND | Ground | GND | Common ground |
| BCK / SCK | Bit Clock | P0.28 | I2S Bit Clock output |
| LRCK / WS | Word Select | P0.29 | I2S LR Clock (44.1 kHz) |
| DIN / SDOUT| Serial Audio Data | P0.30 | Serialized PCM audio data |

### 3.2 MicroSD Card Reader (High-Speed SPIM3)
Connected to dedicated SPIM3 (up to 32 MHz with EasyDMA):

| MicroSD Pin | Signal Name | nRF52840 Pin | Notes |
|:---|:---|:---|:---|
| VCC | Power | 3.3V | MicroSD power |
| GND | Ground | GND | Common ground |
| CS | Chip Select | P0.22 | Active-low Card Select |
| SCK | SPI Clock | P0.23 | SPIM3 Clock (up to 32 MHz) |
| MOSI | Master Out Slave In | P0.24 | Data to MicroSD |
| MISO | Master In Slave Out | P0.25 | Data from MicroSD |

### 3.3 Multi-Display Connections

#### Option A: 0.96" / 1.3" I2C Monochrome OLED (SSD1306 / SH1106)
- Compile flag: `-DDISPLAY_TYPE=OLED_I2C`

| OLED Pin | Signal Name | nRF52840 Pin | Notes |
|:---|:---|:---|:---|
| VCC | Power | 3.3V | Display power |
| GND | Ground | GND | Ground |
| SCL | I2C Clock | P0.27 | TWIM SCL (400 kHz Fast Mode) |
| SDA | I2C Data | P0.26 | TWIM SDA |

#### Option B: 2.8" SPI Color LCD (ST7789 / ILI9341)
- Compile flag: `-DDISPLAY_TYPE=COLOR_LCD`

| LCD Pin | Signal Name | nRF52840 Pin | Notes |
|:---|:---|:---|:---|
| CS | Chip Select | P0.17 | SPI CS |
| DC | Data / Command | P0.16 | Low = Cmd, High = Data |
| SCK | SPI Clock | P0.15 | SPIM SCK |
| MOSI | SPI Data | P0.13 | SPIM MOSI |
| RST | Hardware Reset | P0.14 | Reset pulse |
| BL | Backlight PWM | P0.12 | PWM brightness |

#### Option C: 2.13" / 2.9" SPI E-Ink EPD (SSD1680 / UC8151)
- Compile flag: `-DDISPLAY_TYPE=EINK_EPD`

| EPD Pin | Signal Name | nRF52840 Pin | Notes |
|:---|:---|:---|:---|
| CS | Chip Select | P0.17 | SPI CS |
| DC | Data / Command | P0.16 | Data/Command |
| SCK | SPI Clock | P0.15 | SPI Clock |
| MOSI | SPI Data | P0.13 | SPI Data |
| BUSY | Busy Status | P0.11 | High when refreshing |
| RST | Hardware Reset | P0.14 | Reset pulse |

### 3.4 Navigation Pushbuttons
All pushbuttons connect between the GPIO pin and GND (internal pull-ups enabled):
- Select / Play-Pause: P0.02 (Hold >= 500 ms for Back)
- Next / Down: P0.03
- Prev / Up: P0.04
- Vol+ (Optional): P0.05
- Vol- (Optional): P0.20

### 3.5 Battery Voltage Measurement
- Connected to internal SAADC Channel 0 (`P0.02 / AIN0`) with 1/6 gain and 0.6V reference.

---

## 4. Nordic Semiconductor nRF54L15

The next-generation ultra-low-power ARM Cortex-M33 SoC with hardware Serial Audio Interface (SAI) and native Bluetooth 5.4 LE Audio.

| Interface | Peripheral Pin | Function | Notes |
|:---|:---|:---|:---|
| **I2S DAC** | P1.04 | I2S SCK / BCLK | Bit clock to DAC |
| **I2S DAC** | P1.05 | I2S LRCK / WS | Word select (44.1/48 kHz) |
| **I2S DAC** | P1.06 | I2S SDOUT | Serial audio data |
| **MicroSD** | P2.00..03 | High-speed SPIM | MicroSD SCK, MOSI, MISO, CS |
| **I2C OLED** | P0.04, P0.05 | TWIM SCL / SDA | SSD1306 display bus |
| **SPI LCD** | P0.08..11 | SPIM / GPIO | ST7789 / ILI9341 display bus |
| **Buttons** | P0.14..18 | GPIOTE | Low-power wakeup buttons |
| **Battery** | P0.01 / AIN0 | SAADC | Battery voltage sensing |
| **LE Audio** | Integrated 2.4 GHz | [EXPERIMENTAL] Auracast | Direct LC3 broadcast stream |

---

## 5. mikromedia Plus for STM32F7 (STM32F746ZG @ 216 MHz)

The mikromedia Plus for STM32F7 is an integrated ARM Cortex-M7 board with onboard 4.3" 480×272 TFT, VS1053B audio codec, and high-speed 4-bit SDMMC MicroSD slot.

### 5.1 16-Bit Parallel Display (SSD1963 480×272)

| Signal Name | STM32F7 Pin | Notes |
|:---|:---|:---|
| Data Bus (D0..D15) | PE0 .. PE15 | 16-bit parallel data port |
| TFT_WR (Write) | PF11 | Active-low Write strobe |
| TFT_RD (Read) | PF12 | Active-low Read strobe |
| TFT_CS (Chip Select)| PF13 | Active-low Chip Select |
| TFT_RST (Reset) | PF14 | Active-low Hardware Reset |
| TFT_RS (Data/Cmd) | PF15 | 0 = Command / Address, 1 = Data |
| TFT_BLED (Backlight)| PF10 | Hardware PWM brightness control |

### 5.2 Audio Codec (VS1053B on SPI2)

| Signal Name | STM32F7 Pin | Notes |
|:---|:---|:---|
| SPI2_SCK | PB13 | Serial Clock |
| SPI2_MISO | PB14 | Data from codec |
| SPI2_MOSI | PB15 | Data to codec |
| MP3_CS | PD11 | Control Chip Select (SCI) |
| XDCS / BSYNC | PD10 | Data Chip Select (SDI) |
| DREQ | PD9 | Data Request interrupt |
| MP3_RST | PD8 | Hardware reset |

### 5.3 MicroSD Slot (Native 4-bit SDMMC1)

| Signal Name | STM32F7 Pin | Notes |
|:---|:---|:---|
| SDMMC_D0 | PC8 | Data bit 0 |
| SDMMC_D1 | PC9 | Data bit 1 |
| SDMMC_D2 | PC10 | Data bit 2 |
| SDMMC_D3 | PC11 | Data bit 3 |
| SDMMC_CMD | PD2 | Command line |
| SDMMC_CLK | PC12 | Clock (up to 48 MHz) |
| SD_DETECT | PD3 | Active-low Card Detect switch |

---

## 6. Summary Target Matrix

| Feature | Desktop Simulator | RP2040 / RP2350 | LilyGO T-Display S3 | Nordic nRF52840 | Nordic nRF54L15 | mikromedia Plus STM32F7 |
|:---|:---|:---|:---|:---|:---|:---|
| CPU Core | Host x86_64 / ARM64 | Dual Cortex-M0+ / M33 | Dual Xtensa LX7 @ 240MHz | Cortex-M4F @ 64MHz | Cortex-M33 @ 128MHz | Cortex-M7 @ 216MHz |
| Default Display | SDL2 Window | 320x240 ILI9341 SPI | 320x170 ST7789 Parallel | Multi-Profile (OLED/LCD/EPD)| Multi-Profile (OLED/LCD/EPD)| 480x272 SSD1963 16b Parallel |
| Audio Hardware | SDL2 Audio Queue | PIO I2S + DMA | Hardware I2S + DMA | NRF_I2S + EasyDMA | SAI / I2S + EasyDMA | VS1053B SPI2 + STM32 DAC |
| Wireless Audio | N/A | N/A | BLE Audio (LC3) | [EXP] 2.4G Custom BLE | [EXP] BT 5.4 Auracast | N/A |
| Flash Binary | build/kopuz_sim | build_rp2040/*.uf2 | ESP-IDF .bin | nRF52 .hex / .elf | nRF54 .hex / .elf | STM32F7 .bin / .hex / ST-Link |


