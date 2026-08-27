# Kopuz Device: C/C++ Embedded Music Player

Standalone embedded music-player firmware rewritten in high-performance, deterministic C99 and C++17. Companion hardware player to the Kopuz music player, running on ultra-low-power microcontrollers with no external PSRAM requirements.

---

## 1. Overview and Key Features

- Low Memory Footprint: Complete firmware runs within <= 48 KB RAM (runs in internal SRAM on RP2040 and ESP32-S3 without 8 MB Octal PSRAM).
- Multi-Target Architecture:
  - Desktop Simulator: Full interactive graphical simulator running natively on macOS, Linux, and Windows with SDL2 audio/video.
  - LilyGO T-Display S3 (ESP32-S3): 1.9-inch 170x320 ST7789 8-bit parallel display (landscape 320x170), I2S DAC, and dual-core task scheduling.
  - Raspberry Pi Pico (RP2040) and Pico 2 (RP2350): ILI9341 320x240 SPI display, PIO-driven I2S DAC, and dual-core processing.
- Audio Codec Pipeline:
  - FLAC: dr_flac streaming integer decoder with native FLAC PICTURE metadata extraction.
  - MP3: minimp3 fixed-point streaming decoder with ID3v2 APIC cover art extraction.
  - WAV: Zero-overhead streaming RIFF/WAVE PCM reader.
  - Album Art: stb_image JPEG decompressor with box-sampling downscaling to RGB565.
- User Interface:
  - 1bpp monochrome layout engine with 24 Rusic color themes.
  - Live Now-Playing screen with cover art, track info, shuffle/repeat flags, volume readout, and Up Next queue preview.
  - Full queue browsing across Songs, Albums, Artists, and Settings.
  - In-track seeking (+/- 5 seconds) and button repeat emulation.
  - Real-time RAM usage and battery voltage monitoring.
- System Safety:
  - Blue Screen of Death (BSOD) crash recovery system with dynamic QR code generation.
  - Out-of-memory traps and RAM ceiling monitoring to prevent silent hangs.

---

## 2. Supported Targets and Wiring

See CONNECTIONS.md for complete pinouts, wiring diagrams, and schematics for:
- Raspberry Pi Pico (RP2040) / Pico 2 (RP2350)
- LilyGO T-Display S3 (ESP32-S3)

See DESIGN.md for deep technical architecture, memory budgets, and design specifications.

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
- Enter / Space: Select / Play-Pause
- Enter (Hold >= 500ms): Back
- Down / J: Next track / Scroll down
- Up / K: Prev track / Scroll up
- Right / L: Seek forward 5s
- Left / H: Seek backward 5s
- + / = / U: Volume up
- - / D: Volume down
- Esc / Backspace: Back
- C: Trigger BSOD crash test

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
    ├── rp2040/                 # RP2040 and RP2350 Pico SDK target
    └── simulator/              # Desktop SDL2 simulator target
```
