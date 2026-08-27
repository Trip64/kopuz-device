# Kopuz Device: C/C++ High-Performance Architecture and Redesign Specification

## 1. Executive Summary and Original Project Analysis

### 1.1 What was kopuz-device?
The original Kopuz-org/kopuz-device is an embedded standalone music player companion to the Kopuz/rusic desktop player. It was originally engineered for the Deneyap Kart 1A v2 (ESP32-S3) with either a Waveshare 2.13-inch e-Paper (SSD1680) or an ILI9341 320x240 SPI TFT.

### 1.2 The Original Rust High, C Low Stack
The original implementation had an asymmetric split:
- C Components (components/): Bare-metal ESP-IDF drivers for:
  - epd: Waveshare 2.13-inch e-paper SPI driver with full/partial LUT refresh.
  - ili9341: 320x240 SPI display driver with backlight PWM and RGB565 region blitting.
  - buttons: 5 GPIO ISRs with debouncing and long-press detection on Play/Pause (held >= 500 ms -> Back).
  - audio_out: ESP-IDF i2s_std driver outputting 32-bit interleaved PCM to a PCM5102A DAC (or legacy PWM buzzer).
  - storage: SD card mounted over SPI via SDSPI + FATFS to /sdcard.
  - battery and ldr: ADC one-shot readings with calibration for LiPo voltage and ambient light auto-dimming.
- Rust Core (src/):
  - main.rs: Multi-threaded orchestrator running 3 FreeRTOS threads: Main UI loop (50ms tick), Audio decode loop, and JPEG cover art decode loop.
  - app.rs: State machine managing 8 screens (Menu, NowPlaying, Songs, Albums, AlbumTracks, Artists, ArtistTracks, Settings), volume, shuffle, repeat, 24 themes, and play queue order.
  - audio/decoder.rs: Wrapper over Symphonia (flac and mp3 features enabled) and raw WAV streaming.
  - art.rs: JPEG decoding using jpeg-decoder crate, box-sampling downscale to 48px or 112px, and Floyd-Steinberg dithering for 1bpp.
  - display/mini_player.rs and framebuffer.rs: UI layout using Rust's embedded-graphics into a 1bpp landscape buffer.

### 1.3 Critical Weaknesses of the Original Rust Architecture
1. Excessive Resource Consumption and Dependency on PSRAM:
   - symphonia pulled in extensive heap-allocated media container abstractions and large static decoding tables for MP3/FLAC.
   - Rust standard library (std) and thread allocations required 8 MB Octal PSRAM (CONFIG_SPIRAM_MODE_OCT=y).
   - The thread stacks (64KB audio, 32KB art, plus main thread) starved ESP-IDF's internal DMA-capable SRAM, causing SDMMC DMA alloc failures and forcing Wi-Fi/lyrics removal.
   - Incompatibility with standard ARM Cortex-M microcontrollers: Cannot run on an RP2040 (which has only 264 KB internal SRAM and usually no external PSRAM) or mid-range STM32s.
2. Toolchain Fragility and Build Pain:
   - Xtensa ESP32 target requires custom Rust toolchains (espup, ldproxy), specialized LLVM forks, and fragile environment scripts.
   - Build system friction: cargo build failed to detect C header changes without manual touches.
3. Display Subsystem Overhead:
   - Rendering everything through embedded-graphics 1bpp pipeline meant color TFTs (like ILI9341 or ST7789) had to constantly unpack 1bpp mono bits to 16-bit RGB565 in software before SPI DMA pushing, while color album art required complex out-of-band overlay blits.

---

## 2. The New C/C++ Architecture

The redesign replaces all Rust components with clean, highly optimized, portable C99 / C++17 firmware engineered for deterministic execution, low RAM footprint (<= 48 KB RAM requirement for the entire audio+GUI player), and zero reliance on heavy runtimes.

```
+-------------------------------------------------------------------------+
|                           Kopuz Application UI                          |
|  (Mini-Player View, Now-Playing, Songs, Albums, Artists, Settings, etc.)|
+-------------------------------------------------------------------------+
|       App State Machine       |            Embedded Font Engine         |
| (Queue, Shuffle, Repeat, Vol) |          (6x10 Clean, 8x13 Bold)        |
+-------------------------------+-----------------------------------------+
|                  Core Rendering & Framebuffer Engine                    |
|       - 1bpp Fast Mono Rasterizer (E-Paper / Retro TFT)                 |
|       - RGB565 Direct Color Rasterizer (ST7789 / ILI9341)               |
|       - Floyd-Steinberg Ditherer & 24 Rusic Color Themes                |
+-------------------------------------------------------------------------+
|                             Audio Pipeline                              |
|   - minimp3 (Fixed-Point ARM & Xtensa ASM optimized MP3)                |
|   - dr_flac (Streaming integer FLAC decoder)                            |
|   - Zero-Copy WAV Streaming Engine                                      |
|   - stb_image direct downscaler                                         |
+-------------------------------------------------------------------------+
|                  HAL (Hardware Abstraction Layer)                       |
|   hal_audio.h | hal_display.h | hal_storage.h | hal_input.h | hal_sys.h |
+-------------------------------------------------------------------------+
   |                       |                           |
   v                       v                           v
[RP2040 / RP2350]   [T-Display S3]              [Generic ARM / STM32]
- Dual Cortex-M0+/M33- ESP32-S3 Dual LX7        - Cortex-M4 / M7
- PIO I2S + DMA     - 8-bit Parallel ST7789     - SAI / I2S + DMA
- SPI/SDIO FatFs    - I2S DMA to DAC            - SDMMC + FatFs
- Pico SDK (CMake)  - ESP-IDF (CMake)           - CMSIS / Baremetal
```

### 2.1 Target Platforms
1. RP2040 / RP2350 (Raspberry Pi Pico / Pico 2):
   - CPU: Dual ARM Cortex-M0+ at 133-250 MHz (RP2040) or Dual ARM Cortex-M33 with DSP & FPU at 150-300 MHz (RP2350).
   - RAM: 264 KB (RP2040) / 520 KB (RP2350).
   - Audio Sink: PIO-driven I2S master with DMA double-buffering (yielding bit-perfect 44.1kHz/48kHz 16-bit/32-bit stereo output to PCM5102A or UDA1334A).
   - Display: Hardware SPI0 driving original ILI9341 (320x240) with PWM backlight.
   - Storage: MicroSD over dedicated SPI1 with FatFs.
   - Core Allocation: Core 0 runs UI and Storage; Core 1 runs real-time Audio Decode and DMA feeding.
2. LilyGO T-Display S3 (ESP32-S3):
   - CPU: Dual Xtensa LX7 at 240 MHz.
   - Display: Built-in ST7789V 1.9-inch 170x320 8-bit 8080-I parallel bus using the ESP32-S3 LCD controller with DMA (320x170 landscape).
   - Audio Sink: I2S output on expansion header feeding PCM5102A or MAX98357A.
   - Storage: MicroSD card over SPI/SDMMC.
   - Battery Monitoring: On-board LiPo voltage divider on GPIO4.
3. Generic ARM MCUs (STM32F4/F7/H7, NXP i.MX RT, ATSAMD51):
   - Portable HAL architecture allows linking with standard CMSIS, FatFs, and hardware I2S/SAI DMA channels.

---

## 3. Detailed Component Architecture

### 3.1 Ultra-Optimized Audio Pipeline
Instead of bloated generic decoders, the C/C++ rewrite employs specialized fixed-point/integer decoders:
- MP3 Decoder:
  - minimp3: fixed-point math specifically for ARM Cortex-M and Xtensa. Needs approx 16 KB RAM and executes with low CPU overhead. Includes automatic ID3v2 APIC cover art extraction.
- FLAC Decoder:
  - dr_flac: single-block streaming integer decoder with native FLAC PICTURE metadata extraction.
- WAV Decoder:
  - Lightweight RIFF/WAVE header parser with direct stream pass-through.
- Embedded Cover Art (JPEG):
  - stb_image with direct box downscaling to RGB565 during decompression.

### 3.2 UI Engine & Design Fidelity
The GUI strictly adheres to the original Kopuz design language:
1. Screen Layout:
   - Top Bar (Height 14px): Uppercase screen title (KOPUZ, NOW PLAYING, SONGS, ALBUMS, ARTISTS, SETTINGS), right-aligned battery percentage / USB badge, and 1px separator line.
   - Body Area:
     - Now Playing: Album art box on left, Title/Artist/Album column on right, status flags (SHUF, RPT:1/A/-), scrollable UP NEXT queue list (up to 6 tracks), horizontal progress bar with filled indicator, elapsed/total time (mm:ss / mm:ss), and volume readout (vol XX).
     - List Screens: Multi-row track/album list with active row inversion (text punches through solid highlight bar), playback status glyph (> playing, || paused, [] stopped), up/down carets for multi-page scroll.
   - Footer Mini-Player (List Screens): Separator line, status glyph + track title, shuffle/repeat indicators, miniature progress bar, and timestamp.
2. Typography:
   - Embedded 1bpp bitmap fonts:
     - font_6x10: ASCII font for list items, metadata, times, and tags.
     - font_8x13_bold: Header font for titles and top bar.
3. Color Themes:
   - All 24 Rusic themes (Default, Gruvbox, Gruvbox Soft, Dracula, Nord, Catppuccin, One Dark, Rose Pine, Ayu Dark, Latte, etc.) supported via 16-bit RGB565 color pairs (fg, bg).

### 3.3 Hardware Abstraction Layer (HAL) Definition
All hardware interactions are mediated by clean C header interfaces:
- hal/hal_audio.h
- hal/hal_display.h
- hal/hal_storage.h
- hal/hal_input.h
- hal/hal_power.h
- hal/hal_system.h

---

## 4. Multi-Target Compilation Strategy
The build system uses modern CMake with modular targets:
- targets/simulator: Native desktop simulator (SDL2)
- targets/rp2040: Raspberry Pi Pico SDK (RP2040 and RP2350)
- targets/esp32s3_tdisplay: ESP-IDF for LilyGO T-Display S3
