# kopuz-device

Standalone music-player firmware for the **Deneyap Kart 1A v2** (ESP32-S3),
with a **Waveshare 2.13" e-Paper HAT (V2)** display. Companion hardware to the
[Kopuz](../rusic) music player — same mini-player idea, on a tiny screen.

## Design: Rust high, C low

| Layer | Language | Where |
|-------|----------|-------|
| Audio decode, library scan, mini-player UI, app state | **Rust** | `src/` |
| E-paper SPI driver, button GPIO/ISR, audio sink (PWM/I2S) | **C** | `components/` |

The C bits are real **ESP-IDF components** (full SDK: SPI, GPIO, I2S, FreeRTOS).
`esp-idf-sys` compiles them and runs bindgen over `src/bindings.h`, so Rust
calls the C API as `esp_idf_sys::*` — wrapped safely in `src/ffi.rs`. That is
the whole FFI boundary.

```
src/
  main.rs          entry + 2 threads (UI loop, audio loop)
  ffi.rs           safe wrappers over the C components
  app.rs           queue / playback state machine + button handling
  library/         SD card scan -> Track list
  audio/           decode pipeline (WAV hand-rolled; FLAC via symphonia)
  display/         framebuffer (landscape) + embedded-graphics track-list UI
  sd.rs            calls the C storage mount
components/
  epd/             Waveshare 2.13" SSD1680 driver (full + partial refresh)
  buttons/         5 debounced active-low buttons (GPIO ISR)
  audio_out/       PWM speaker backend now, I2S DAC later (one #define)
  storage/         SD card SDSPI -> FATFS mount at /sdcard
```

## Toolchain

Xtensa needs the espup-managed Rust fork:

```sh
cargo install espup espflash ldproxy
espup install
. ~/export-esp.sh        # run in each shell (or source from your profile)
```

## Build & flash

```sh
. ~/export-esp.sh                 # once per shell — sets up the esp toolchain

cargo build --release             # build the firmware ELF
cargo run --release               # build + flash + open serial monitor
```

Or flash an existing build explicitly:

```sh
espflash flash --port /dev/cu.usbmodem2101 --no-skip \
    target/xtensa-esp32s3-espidf/release/kopuz-device
```

First build downloads + compiles ESP-IDF (`v5.3.1`, pinned in `.cargo/config.toml`) — slow once, cached after.

### ⚠️ Changed a C file? You MUST force a rebuild

This is the #1 footgun. **`cargo build` does NOT recompile the C components by
itself.** `esp-idf-sys` only re-runs its CMake/Ninja build when **`src/bindings.h`**
changes. So if you edit anything under `components/` (e.g. `audio_out.c`,
`epd.c`, `buttons.c`, `storage.c`), Cargo happily relinks the *old* compiled C
and your change is silently absent from the firmware.

**Rule: after any change under `components/`, touch the header first:**

```sh
touch src/bindings.h && cargo build --release
```

How to tell it actually rebuilt the C:

- **~5–8s build** → only Rust recompiled, **C is stale** (your C change is NOT in).
- **~25s+ build** → Ninja recompiled the C component. Good.

If unsure, just always run the `touch` line — it's free when nothing changed.
For a full from-scratch C rebuild (rarely needed): `cargo clean && cargo build --release`
(re-downloads/recompiles all of ESP-IDF — many minutes).

> Editing only Rust (`src/*.rs`, except `bindings.h`)? Plain `cargo build` is fine.

### Gotchas (Deneyap native-USB board)

- **`espflash` leaves the board in download mode**, and a bootlooping board is
  hard to grab. If flashing fails to connect, force download mode: **hold BOOT,
  tap RST (or replug USB) while holding, release BOOT**, then flash with
  `--before no-reset`.
- **The app only runs after a physical power-cycle** (unplug→replug). espflash's
  reset drops to download mode here.
- **Read serial without resetting:** `cat /dev/cu.usbmodem2101` (the `cu` device
  doesn't toggle DTR). `espflash monitor` tends to strap it into download mode.

## Controls

5 buttons (wire each between the GPIO and GND; internal pull-ups, active-low):

| Button | Action | GPIO | Silkscreen |
|--------|--------|------|------------|
| **Select / Play-Pause** | Play the highlighted track; pause/resume if it's the current one | GPIO4 | A0 |
| **Back / Up** | Scroll the track list up | GPIO6 | A2 |
| **Forward / Down** | Scroll the track list down | GPIO5 | A1 |
| **Vol +** | Volume up | GPIO7 | A3 |
| **Vol −** | Volume down | GPIO16 | A5 |

> To re-map which physical button is which, edit the `PIN_*` defines at the top
> of `components/buttons/buttons.c`.

## Usage

1. Format an SD card as **FAT32**, copy `.flac` or `.mp3` files onto it, insert it.
   (The scan ignores dotfiles and macOS `._*` AppleDouble sidecars — those would
   otherwise poison the queue with un-decodable junk.)
2. Power on. The screen shows: **album-art box + now-playing on the left**, your
   **scrollable track list on the right** (landscape).
3. **Back/Forward** scroll the list, **Select** plays the highlighted track.
   The progress/time updates on screen every few seconds while playing.

Audio comes out the PWM pin (GPIO15) into the active speaker. FLAC and MP3
decode today (via symphonia); a hand-rolled WAV reader also exists but the
library scan only picks up `.flac`/`.mp3`.

## Wiring (Deneyap Kart 1A v2)

GPIO numbers below are the real ESP32-S3 GPIOs from the board variant; the
silkscreen label (what's printed on the board) is in parentheses. Change them
in the files listed under each table.

### Waveshare 2.13" e-Paper HAT (V2) — `components/epd/epd_pins.h`

The HAT has an 8-pin header. Wire it to the board's **main SPI bus** + 4 free
control pins:

| HAT pin | Connect to GPIO | Silkscreen | Notes |
|---------|-----------------|------------|-------|
| VCC     | 3V3             | 3V3        | 3.3 V only |
| GND     | GND             | GND        | |
| DIN (MOSI) | **GPIO39**   | D7 / MOSI  | SPI data |
| CLK (SCK)  | **GPIO41**   | D5 / SCK   | SPI clock |
| CS      | **GPIO42**      | D4 / SS    | chip select |
| DC      | **GPIO2**       | D1         | data/command |
| RST     | **GPIO1**       | D0         | reset |
| BUSY    | **GPIO8**       | D14        | busy (input) |

(The HAT's MISO is not used by e-paper.)

### Deneyap Hoparlör (active speaker, PAM8302A) — `components/audio_out/audio_out.c`

Use the dedicated speaker port for power; feed the audio signal from one PWM pin:

| Speaker pin | Connect to | Notes |
|-------------|-----------|-------|
| VCC / 3V3   | 3V3       | via the JST power connector |
| GND         | GND       | |
| IN (signal) | **GPIO15** (A4) | PWM today, swap to I2S DAC later |

### Buttons (×5, to GND) — `components/buttons/buttons.c`

See the **Controls** section above for the button → GPIO map and what each does.

### SD card (music storage) — `components/storage/storage.c`

Uses the board's dedicated SD header (separate SPI3 bus). Pins are defined in
`components/storage/storage.c`:

| SD pin | GPIO | Silkscreen |
|--------|------|------------|
| MOSI   | GPIO12 | SDMO |
| MISO   | GPIO14 | SDMI |
| CLK    | GPIO13 | SDCK |
| CS     | GPIO11 | SDCS |

> This board has **8 MB octal PSRAM** — `sdkconfig.defaults` sets
> `CONFIG_SPIRAM_MODE_OCT=y`. Quad mode would hang at boot on this octal chip.

## Notes / next steps

- **Formats:** FLAC + MP3 (symphonia, with the `flac`+`mp3` features). The board
  has 8 MB octal PSRAM so the mp3 tables fit. Vorbis is still off.
- **Audio out:** the PWM backend mirrors the proven Deneyap BadApple engine — a
  40 kHz LEDC carrier on GPIO15 plus a **gptimer ISR firing at the sample rate**
  that pops 8-bit samples from a ring buffer and writes the LEDC duty directly
  (`ledc_ll`, IRAM-safe). `audio_out_write` is just the feeder. For real fidelity,
  flip `AUDIO_BACKEND_I2S` in `components/audio_out/audio_out.c` and implement the
  `i2s_channel_write` path when an I2S DAC is wired.
- **E-paper:** full refresh on first paint + every ~10th, partial (flicker-free)
  in between for list scrolling.
