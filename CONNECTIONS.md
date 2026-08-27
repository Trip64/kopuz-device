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

## 3. Summary Comparison Table

| Feature | Desktop Simulator | RP2040 / RP2350 | LilyGO T-Display S3 |
|:---|:---|:---|:---|
| Display Panel | SDL2 Native Window | ILI9341 2.8-inch SPI TFT (Original) | ST7789V 1.9-inch 8-bit Parallel (Onboard) |
| Display Resolution | 320x170 (or 320x240) | 320 x 240 (Original QVGA) | 320 x 170 (Wide Landscape) |
| Color Depth | 16-bit RGB565 | 16-bit RGB565 | 16-bit RGB565 |
| Audio Output | SDL2 Audio Queue | Hardware PIO I2S + DMA | Hardware ESP-IDF I2S + DMA |
| Threading Model | Main loop + timer | Core 0 (UI) / Core 1 (Audio) | FreeRTOS Dual-Core Tasks |
| Flash Binary | build/kopuz_sim | build_rp2040/kopuz_rp2040.uf2 | ESP-IDF .bin |
