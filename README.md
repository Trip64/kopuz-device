# Kopuz Device

Kopuz Device is a portable C99 firmware core for a small standalone music player. The repository includes an SDL2 desktop simulator, streaming WAV/MP3/FLAC decoders, a compact framebuffer UI, and hardware-abstraction layers for several microcontroller families.

The simulator is the reference implementation today. Hardware targets are under active development and still need validation on their named boards.

## Project status

| Target | Status | Notes |
| --- | --- | --- |
| Desktop simulator | Tested | Built and exercised in CI with SDL2, sanitizers, unit tests, and a playback smoke test. |
| Elecrow CrowPanel 2.4 V2.1 | Partial hardware test | Display, SD, serial control, and conservative flashing were checked on a board. Touch previously failed; its detection and UI have been revised but need a physical retest. A2DP still needs a pairing/playback test. |
| LilyGO T-Display S3 | Experimental | ESP-IDF project and drivers are present; requires on-device build and electrical validation. |
| RP2040 / RP2350 | Experimental | Display/audio foundations are present; MicroSD directory support is not complete. |
| STM32F746 mikromedia | Integration scaffold | Board-specific drivers are present, but the repository does not yet provide a complete vendor SDK/toolchain package. |
| nRF52840 | Integration scaffold | Several HAL implementations are placeholders and require a Nordic SDK integration. |
| nRF54L15 | Planned | Architecture notes only. |

Do not treat an untested hardware target as production-ready. Contributions with board logs, measurements, and reproducible toolchain versions are especially valuable.

## Highlights

- Portable application state machine and hardware abstraction layer.
- SDL2 simulator with seven display profiles and multiple DAC models.
- Streaming PCM WAV, MP3 (`minimp3`), and FLAC (`dr_flac`) decoding.
- JPEG album-art decoding and RGB565 downscaling.
- Songs, albums, artists, settings, Bluetooth, now-playing, and crash screens.
- Deterministic library ordering, bounded recursive scanning, and allocation-failure handling.
- CTest coverage for playback state, settings persistence, library scanning, non-canonical WAV files, and malformed MP3/FLAC streams.
- AddressSanitizer and UndefinedBehaviorSanitizer support.

## Build the simulator

Requirements:

- CMake 3.15 or newer
- A C99 compiler
- SDL2 development files
- Python 3 for generating local smoke-test audio
- FFmpeg for MP3 and FLAC smoke-test fixtures

Install SDL2:

```sh
# macOS
brew install cmake sdl2 ffmpeg

# Ubuntu / Debian
sudo apt-get install cmake libsdl2-dev ffmpeg
```

Configure and build:

```sh
cmake -S . -B build -DKOPUZ_TARGET=SIMULATOR -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Run the automated tests:

```sh
ctest --test-dir build --output-on-failure
```

Run the simulator:

```sh
./build/kopuz_sim
```

Generate local audio fixtures and run the playback smoke test:

```sh
python3 tools/create_test_audio.py
SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy ./build/kopuz_sim --test
```

The generated `sdcard/` library and `eeprom.bin` settings file are local runtime data and are ignored by Git.

### Runtime safety checks

For development, enable AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build-sanitize \
  -DKOPUZ_TARGET=SIMULATOR \
  -DKOPUZ_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## Simulator controls

| Input | Action |
| --- | --- |
| Enter / Space | Select or play/pause |
| Enter, held for 500 ms | Back |
| Up / K | Previous track or move up |
| Down / J | Next track or move down |
| Left / H | Seek backward five seconds |
| Right / L | Seek forward five seconds |
| `+` / `=` / U | Volume up |
| `-` / D | Volume down |
| Escape / Backspace | Back |
| F1–F7 / Tab | Select or cycle display profiles |
| F8 | Print DAC and buffer diagnostics |
| F9 | Cycle DAC models |
| C | Open the crash-screen demo |

Use `./build/kopuz_sim --help` to list command-line options.

## ESP32-S3 build

The T-Display S3 target is a complete ESP-IDF project rooted at `targets/esp32s3_tdisplay`. With ESP-IDF v5 installed and exported:

```sh
cd targets/esp32s3_tdisplay
idf.py set-target esp32s3
idf.py build
```

Flashing is intentionally left as a separate step so the serial port is explicit:

```sh
idf.py -p /dev/ttyACM0 flash monitor
```

This target still requires physical validation before release use.

## CrowPanel 2.4-inch build

The Elecrow DIS03024H V2.1 target is under
[`targets/esp32_crowpanel_24`](targets/esp32_crowpanel_24). It uses the classic
ESP32-WROOM-32-N4, separate SPI controllers for its ILI9341V display and SD
card, a conservative 4 MHz SD clock, and 115200-baud uploads. See the target
README for the exact build, flashing, card preparation, and recovery steps.

## Other hardware targets

The RP2040/RP2350, nRF52840, and STM32F7 directories contain useful board code, but they are not yet covered by CI or hardware-in-the-loop tests. See [CONNECTIONS.md](CONNECTIONS.md) for the intended wiring and [DESIGN.md](DESIGN.md) for the architecture and current constraints.

## Memory model

RAM use depends on the selected display, track limit, decoder, and hardware SDK. The firmware does not currently guarantee a 48 KB total ceiling.

Important contributors include:

- `sizeof(track_t) * MAX_TRACKS` for the library queue;
- the 1-bit framebuffer;
- the RGB565 album-art buffer;
- decoder state and PCM buffers;
- platform driver and RTOS allocations.

The emergency queue now holds only eight tracks instead of duplicating the full configured queue. Before claiming support for a constrained board, measure the linked image and peak heap on that exact target.

## Repository layout

```text
.
├── .github/workflows/ci.yml       # Simulator CI and sanitizer checks
├── CMakeLists.txt                 # Desktop build and tests
├── CONNECTIONS.md                 # Intended hardware wiring
├── DESIGN.md                      # Architecture and constraints
├── hal/                           # Portable hardware interfaces
├── include/                       # Shared application headers
├── src/                           # Portable application, UI, and codecs
├── targets/                       # Simulator and board integrations
├── tests/                         # Native regression tests
└── tools/                         # Development utilities
```

## Near-term roadmap

1. Complete CrowPanel A2DP pairing tests and enable its onboard DAC speaker.
2. Validate and stabilize the ESP32-S3 build on physical hardware.
3. Finish RP2040 MicroSD/FatFs integration and add a reproducible SDK build.
4. Expand committed MP3 and FLAC fixture coverage across sample rates and metadata variants.
5. Measure stack, heap, underruns, and power on each supported board.
6. Separate platform settings persistence into explicit NVS/flash/SD backends.
