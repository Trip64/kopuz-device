# kopuz-device

Standalone music-player firmware for the **Deneyap Kart 1A v2** (ESP32-S3),
with a **Waveshare 2.13" e-Paper HAT (V2)** display. Companion hardware to the
[Kopuz](../rusic) music player — same mini-player idea, on a tiny screen.

## Design: Rust high, C low

| Layer | Language | Where |
|-------|----------|-------|
| Audio decode, library scan, mini-player UI, app state | **Rust** | `src/` |
| E-paper SPI driver, button GPIO/ISR, audio sink (I2S DAC) | **C** | `components/` |

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
  audio_out/       I2S DAC (PCM5102A) backend; PWM speaker fallback (one #define)
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

### Display backend

Default is the Waveshare 2.13" e-paper. To build for the ILI9341 320×240 TFT
instead, enable the `ili9341` cargo feature:

```sh
cargo run --release --features ili9341
```

The feature only flips which panel the firmware drives (Rust `cfg`); both C
drivers are always compiled. Wire the matching display — see *Wiring* below.

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

Audio comes out the PCM5102A I2S DAC's 3.5 mm stereo jack (BCK=GPIO47,
LCK=GPIO18, DIN=GPIO21). FLAC and MP3 decode today (via symphonia); a
hand-rolled WAV reader also exists but the library scan only picks up
`.flac`/`.mp3`.

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

### ILI9341 320×240 SPI TFT (no touch) — `components/ili9341/ili9341_pins.h`

Alternative display to the e-paper. Build with `--features ili9341` (see
*Display backend* above). Shares the same SPI2 pins as the e-paper header, so
you wire **either** the e-paper **or** the TFT — not both. SDO/MISO and the
touch pins are unused.

| TFT pin | Connect to GPIO | Silkscreen | Notes |
|---------|-----------------|------------|-------|
| VCC     | 3V3             | 3V3        | 3.3 V (most ILI9341 boards have a regulator; 5 V also ok then) |
| GND     | GND             | GND        | |
| SDI/MOSI | **GPIO39**     | D7 / MOSI  | SPI data |
| SCK     | **GPIO41**      | D5 / SCK   | SPI clock |
| CS      | **GPIO42**      | D4 / SS    | chip select |
| DC/RS   | **GPIO2**       | D1         | data/command |
| RST     | **GPIO1**       | D0         | reset |
| LED/BL  | **GPIO8**       | D14        | backlight (driven high = on) |

SPI runs at 40 MHz. The 1bpp UI is rendered full-screen and expanded to RGB565
(white background / black text) — monochrome look on the colour panel for now.

### PCM5102A I2S DAC (stereo line-out) — `components/audio_out/audio_out.c`

Real I2S DAC; replaces the old PWM speaker. Stereo audio comes out the module's
3.5 mm jack. Pins are the `I2S_PIN_*` defines at the top of `audio_out.c`:

| DAC pin | Connect to GPIO | Silkscreen | Notes |
|---------|-----------------|------------|-------|
| VIN     | 5V              | 5V / VIN   | onboard regulator drops to 3.3 V for the codec |
| GND     | GND             | GND        | |
| BCK     | **GPIO47**      | SDA        | bit clock |
| LCK     | **GPIO18**      | A7         | LRCLK / word select |
| DIN     | **GPIO21**      | SCL        | serial data out from ESP |
| SCK     | **GND**         | GND        | **must** tie low → module derives MCLK from BCK via internal PLL |
| XSMT    | 3V3             | 3V3        | soft-unmute; most purple boards pull this high already |
| FLT/DEMP/FMT | GND        | GND        | leave at board defaults |

> **GPIO15 (silk A4) is NOT used for audio** — it doubles as the ILI9341
> backlight in this build, so driving I2S BCK on it dimmed the screen. BCK moved
> to GPIO47 (SDA). SDA/SCL are the I2C pins, free here since no I2C is used.
> **Removed the old PWM speaker.** To fall back to the PWM buzzer, flip
> `AUDIO_BACKEND_I2S` → `AUDIO_BACKEND_PWM` in `audio_out.c` (note: PWM uses GPIO15,
> which now conflicts with the backlight). If these pins aren't broken out on your
> board silk, edit the `I2S_PIN_*` defines.

### Battery (LiPo) — `components/battery/battery.c`

The board has a LiPo connector + charger; battery voltage is sensed on **GPIO9**
(`BAT_VOLT_PIN`, ADC1_CH8) through an on-board 2:1 divider. Plug a single-cell
LiPo into the board's battery JST — no extra wiring. The header shows an icon +
percentage; with no pack (USB only) it reads `USB`. If your board's divider
ratio differs, adjust `BAT_DIVIDER` in `components/battery/battery.c`.

### LDR ambient-light sensor (auto-brightness) — `components/ldr/ldr.c`

Optional. Dims the **ILI9341 TFT backlight** to match the room (no effect on the
e-paper build — reflective panel, no backlight). Sensed on **GPIO17 = ADC2_CH6**
(ADC2 because the battery owns the ADC1 unit; GPIO17 dodges the SD bus, speaker,
buttons, and native-USB pins). With no LDR wired the firmware probes once at boot
and leaves the backlight at full.

An LDR has 2 legs and only changes resistance — pair it with **one separate fixed
resistor** (~10 kΩ) to make a voltage divider the ADC can read. Three wires:

| Wire | From | To | Silkscreen |
|------|------|-----|-----------|
| 1 | LDR leg A | 3V3 | 3V3 |
| 2 | LDR leg B **+** Rfixed leg A (joined) | **GPIO17** | D8 |
| 3 | Rfixed leg B | GND | GND |

```
3V3 ── LDR ──┬── GPIO17 (ADC2_CH6)   <- ADC reads this junction
             └── Rfixed ── GND
```

Bright light lowers LDR resistance → pin voltage rises → brighter backlight.
Wired the other way? Invert the map in `src/main.rs`. Tune `DARK_RAW`/`BRIGHT_RAW`
and `MIN_PCT`/`MAX_PCT` there to your room.

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

## Album art

Embedded cover (FLAC PICTURE / MP3 ID3 APIC, **JPEG only**) is decoded
**downscaled to ≤128px** (`art.rs` `scale(128,128)` — full-res decode OOMs the
heap), then dithered to 1bpp for the Now Playing box. No embedded art / PNG →
placeholder.

> **WiFi + online lyrics were removed.** The WiFi stack reserved so much
> internal DMA RAM that SD reads failed (`esp_dma_capable_malloc: Not enough
> heap` → `sdmmc_read_blocks failed`) and threads couldn't spawn. Lyrics were
> WiFi's only consumer, so both are gone. (Local `.lrc` could be re-added
> offline later if wanted.)

## Bluetooth audio — not supported (hardware)

The ESP32-**S3** has **BLE only — no Bluetooth Classic / no A2DP**, so it
*cannot* stream audio to Bluetooth headphones or speakers. A2DP needs Classic
BT, which only the original ESP32 has. For wireless out you'd add an external
BT-audio transmitter fed over I2S. Audio here is the wired PCM5102A I2S DAC.

## Notes / next steps

- **Formats:** FLAC + MP3 (symphonia, with the `flac`+`mp3` features). The board
  has 8 MB octal PSRAM so the mp3 tables fit. Vorbis is still off.
- **Audio out:** PCM5102A I2S DAC via the IDF `i2s_std` driver (16-bit stereo,
  Philips format, MCLK unused — SCK tied to GND for the module's internal PLL).
  `audio_out_write` attenuates by volume, duplicates mono → L+R, and blocks on
  `i2s_channel_write`. The old 40 kHz LEDC-PWM buzzer backend (GPIO15 + gptimer
  ISR + ring buffer, from the Deneyap BadApple engine) is kept under
  `AUDIO_BACKEND_PWM` in `components/audio_out/audio_out.c` as a fallback.
- **E-paper:** full refresh on first paint + every ~10th, partial (flicker-free)
  in between for list scrolling.
