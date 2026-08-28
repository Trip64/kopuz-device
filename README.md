# Kopuz Device: C/C++ Embedded Music Player

Standalone embedded music-player firmware written in high-performance, deterministic C99 and C++17. Companion hardware player to the Kopuz music player, running on ultra-low-power microcontrollers with no external PSRAM requirements.

---

## 1. Overview and Key Features

- **Low Memory Footprint**: Complete firmware runs within <= 48 KB RAM (runs in internal SRAM on RP2040, ESP32-S3, and Nordic nRF52840 without external PSRAM).
- **Multi-Target Architecture**:
  - **Desktop Simulator**: Full interactive graphical simulator running natively on macOS, Linux, and Windows with SDL2 audio/video.
  - **Raspberry Pi Pico (RP2040) & Pico 2 (RP2350)**: ILI9341 320x240 SPI display, PIO-driven I2S DAC, and dual-core processing.
  - **LilyGO T-Display S3 (ESP32-S3)**: 1.9-inch 170x320 ST7789 8-bit parallel display, hardware I2S DAC, and BLE audio broadcast.
  - **Nordic Semiconductor (nRF52840 & nRF54L15)**: Hardware `NRF_I2S` audio engine, 32 MHz EasyDMA MicroSD storage, and ultra-low-power standby (< 1.5 uA).
  - **mikromedia Plus for STM32F7 (STM32F746ZG)**: High-performance 216 MHz ARM Cortex-M7 with 4.3" 480×272 16-bit parallel TFT (SSD1963), VS1053B audio codec, and native 4-bit SDMMC.
- **Multi-Display Profile Support**:
  - `COLOR_LCD` (480x272 / 320x240 / 320x170): 16-bit RGB565 backlit color display with full album art decoding (SSD1963, ST7789, ILI9341).
  - `OLED_I2C` (128x64): Pure monochrome 1bpp layout for low-cost 0.96" / 1.3" I2C OLEDs (SSD1306, SH1106).
  - `EINK_EPD` (250x122 / 296x128): Ultra-low-power sunlight-readable electronic paper display (SSD1680, UC8151).
  - `SHARP_MIP` (400x240): Reflective Memory-in-Pixel display.
- **Audio Codec Pipeline**:
  - **FLAC**: dr_flac streaming integer decoder with native FLAC PICTURE metadata extraction.
  - **MP3**: minimp3 fixed-point streaming decoder with ID3v2 APIC cover art extraction.
  - **WAV**: Zero-overhead streaming RIFF/WAVE PCM reader.
  - **Album Art**: stb_image JPEG decompressor with box-sampling downscaling to RGB565.
- **User Interface**:
  - 1bpp monochrome layout engine with 24 Rusic color themes.
  - **2-Column Hi-Fi Layout**: Cover art box, 8-band segmented audio equalizer with floating peak-hold physics, format quality badge (`[FLAC 16b/44k]`), and Up Next track queue.
  - **Expandable Settings**: Toggleable visualizer, audio output mode (`I2S DAC` vs `BLE AUDIO`), shuffle, repeat, volume slider, brightness, and themes.
- **System Safety**:
  - Blue Screen of Death (BSOD) crash recovery system with dynamic QR code generation.
  - Memory ceiling monitoring and RAM leak traps.

---

## 2. Supported Targets and Wiring

See [CONNECTIONS.md](CONNECTIONS.md) for complete pinouts, wiring diagrams, and schematics for:
- Raspberry Pi Pico (RP2040) / Pico 2 (RP2350)
- LilyGO T-Display S3 (ESP32-S3)
- Nordic Semiconductor nRF52840 & nRF54L15
- mikromedia Plus for STM32F7 (STM32F746ZG)
- I2C OLED (SSD1306), SPI Color LCD (ST7789/ILI9341), and E-Ink (SSD1680)

See [DESIGN.md](DESIGN.md) for deep technical architecture, memory budgets, and design specifications.

---

## 3. Building and Running

### 3.1 Native Desktop Simulator

Requires CMake and SDL2:

```sh
# macOS
brew install cmake sdl2

# Ubuntu / Debian
sudo apt-get install cmake libsdl2-dev
```

Build and run:

```sh
mkdir build && cd build
cmake .. -DKOPUZ_TARGET=SIMULATOR
cmake --build .

# Run simulator
./kopuz_sim

# Run automated self-test suite
./kopuz_sim --test
```

Controls in Simulator:
- `Enter` / `Space`: Select / Play-Pause
- `Enter` (Hold >= 500ms): Back
- `Down` / `J`: Next track / Scroll down
- `Up` / `K`: Prev track / Scroll up
- `Right` / `L`: Seek forward 5s
- `Left` / `H`: Seek backward 5s
- `+` / `=` / `U`: Volume up
- `-` / `D`: Volume down
- `Esc` / `Backspace`: Back
- `C`: Trigger BSOD crash test

### 3.2 Raspberry Pi Pico / Pico 2 (RP2040 / RP2350)

Requires the Raspberry Pi Pico SDK:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk

# Build for RP2040
mkdir build_rp2040 && cd build_rp2040
cmake .. -DKOPUZ_TARGET=RP2040 -DPICO_PLATFORM=rp2040
cmake --build . -j4

# Build for RP2350 (Pico 2)
mkdir build_rp2350 && cd build_rp2350
cmake .. -DKOPUZ_TARGET=RP2040 -DPICO_PLATFORM=rp2350
cmake --build . -j4
```

Flash the generated `kopuz_rp2040.uf2` by holding the BOOTSEL button while plugging in the board.

### 3.3 LilyGO T-Display S3 (ESP32-S3)

Requires ESP-IDF v5.x:

```sh
cd targets/esp32s3_tdisplay
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### 3.4 Nordic Semiconductor (nRF52840)

Requires `arm-none-eabi-gcc`:

```sh
cd targets/nrf52840
cmake -B build -DKOPUZ_TARGET=NRF52840
cmake --build build
```

### 3.5 mikromedia Plus for STM32F7 (STM32F746ZG)

Requires `arm-none-eabi-gcc`:

```sh
cd targets/stm32f7_mikromedia
cmake -B build -DKOPUZ_TARGET=STM32F7
cmake --build build
```

Flash via ST-Link:
```sh
openocd -f interface/stlink.cfg -f target/stm32f7x.cfg -c "program build/kopuz_stm32f7.elf verify reset exit"
```

---

## 4. Directory Structure

```
.
├── CMakeLists.txt              # Root CMake build configuration
├── CONNECTIONS.md              # Complete hardware pinouts and schematics
├── DESIGN.md                   # Technical specification and architecture
├── README.md                   # Project overview and build guide
├── hal/                        # Hardware Abstraction Layer interfaces
│   ├── hal_audio.h
│   ├── hal_display.h
│   ├── hal_input.h
│   ├── hal_power.h
│   ├── hal_storage.h
│   └── hal_system.h
├── include/                    # Core definitions, themes, and configuration
│   ├── app.h
│   ├── audio_player.h
│   ├── config.h
│   ├── font.h
│   ├── framebuffer.h
│   ├── themes.h
│   └── ui.h
├── src/                        # Core portable application code
│   ├── app.c                   # State machine and button dispatch
│   ├── audio_player.c          # Audio playback pipeline
│   ├── codecs/                 # FLAC, MP3, WAV, and JPEG decoders
│   ├── fonts/                  # 6x10 and 8x13 bold bitmap fonts
│   ├── library/                # FATFS music directory scanner
│   └── ui/                     # Framebuffer renderer, mini-player, BSOD, QR
└── targets/                    # Target-specific implementations
    ├── esp32s3_tdisplay/       # LilyGO T-Display S3 ESP-IDF target
    ├── nrf52840/               # Nordic nRF52840 target
    ├── nrf54l15/               # Nordic nRF54L15 target architecture
    ├── rp2040/                 # RP2040 and RP2350 Pico SDK target
    ├── simulator/              # Desktop SDL2 simulator target
    └── stm32f7_mikromedia/     # mikromedia Plus for STM32F7 target
```
