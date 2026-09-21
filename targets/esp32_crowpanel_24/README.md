# CrowPanel 2.4-inch demo (DIS03024H V2.1)

This target boots the real Kopuz interface on the Elecrow 2.4-inch CrowPanel,
scans a FAT32 microSD card, and lets you navigate with the two on-board keys.
It targets the classic `ESP32-WROOM-32-N4`, not an ESP32-S3.

## What works

- 320x240 landscape Kopuz interface on the ILI9341V display
- PWM backlight control
- microSD mounting and recursive MP3, FLAC, and WAV library discovery
- two-button navigation
- XPT2046 resistive-touch gesture navigation
- non-blocking 115200-baud serial remote control
- Classic Bluetooth A2DP output to headphones and speakers
- album-art decoding and display
- bounded album-art memory and hardened MP3/FLAC streaming
- conservative, reliable flashing and SD settings

The on-board GPIO26 speaker is deliberately muted in this first build. Tracks
decode and the player UI advances at real time, but proper DAC audio is the next
hardware step. Touch, serial control, and the two physical keys all feed the
same application navigation path.

## Bluetooth audio

Open **Bluetooth** from the main menu to start discovery, wait for nearby audio
devices to appear, then select a headphone or speaker. Kopuz is an A2DP source
and advertises as `Kopuz by Trip64`. Pairing supports Secure Simple Pairing and
the common legacy PIN `1234`.

Bluetooth starts only when the Bluetooth screen or a Bluetooth serial command
is used, preserving decoder memory during local playback. Input audio is
converted to stereo 44.1 kHz for A2DP, including mono and 32/48/96 kHz tracks.
Put headphones or speakers into pairing mode before scanning. Unnamed Classic
Bluetooth inquiry results are retained and resolved after discovery when possible.

## Pin map

| Function | Pins |
|---|---|
| ILI9341V / HSPI | MOSI 13, MISO 12, SCLK 14, CS 15, DC 2 |
| Backlight | GPIO 27 |
| XPT2046 touch | CS 33, IRQ 39 (shares LCD SPI) |
| microSD / VSPI | MOSI 23, MISO 19, SCLK 18, CS 5 |
| Buttons | GPIO 25 and GPIO 32, active high |
| Speaker | GPIO 26 |

LCD and SD use separate ESP32 SPI controllers. The LCD starts at Elecrow's
conservative 16 MHz V2.1 example speed; SD starts at 4 MHz to prioritize card
compatibility. The firmware keeps LCD, touch, and SD chip-select lines high
until their buses are ready.

## Build and flash with ESP-IDF

ESP-IDF 5.3 or newer is recommended.

```sh
cd targets/esp32_crowpanel_24
idf.py set-target esp32
idf.py build
idf.py -p /dev/cu.YOUR_PORT -b 115200 flash monitor
```

Replace the sample macOS port with the port on your computer. Use `COM3`-style
names on Windows or `/dev/ttyUSB0`-style names on Linux. Exit the monitor with
`Ctrl+]`.

The V2.1 board has Elecrow's upgraded automatic download circuit. If connection
still stalls at `Connecting...`, hold **BOOT**, tap **RESET**, release **BOOT**,
and retry at 115200 baud. Close any serial monitor before uploading.

## Build and flash with PlatformIO

From the repository root:

```sh
pio run -d targets/esp32_crowpanel_24
pio run -d targets/esp32_crowpanel_24 -t upload --upload-port /dev/cu.YOUR_PORT
pio device monitor -b 115200 -p /dev/cu.YOUR_PORT
```

`platformio.ini` fixes both upload and monitor speed at 115200, uses DIO flash
mode, 40 MHz flash frequency, and the board's 4 MB flash size.

## microSD checklist

1. Start with a 4-32 GB card using an MBR partition table and FAT32 filesystem.
   exFAT is not enabled.
2. Copy a few `.wav`, `.mp3`, or `.flac` files to the card. Artist/album folders
   are optional; the scanner also accepts files in the root.
3. Insert the card while power is off, then power-cycle the board.
4. Watch the first screen. It reports either `SD OK` with the number of tracks,
   or `SD failed`; the serial log includes the exact ESP-IDF error.
5. After a failed initialization, fully remove power before retrying. An SD card
   can remain in SPI mode until its next power cycle.

The firmware never formats a card automatically. If 4 MHz is reliable, increase
`SD_CLOCK_KHZ` in `hal_crowpanel_storage.c` to 10000, then 20000, testing after
each change.

## Controls

| Key | Short press | Long press |
|---|---|---|
| Left / GPIO25 | Next item | Previous item |
| Right / GPIO32 | Select / play-pause | Back |

Touch uses Elecrow's calibrated landscape coordinates. Tap a list row to open
it, or use the on-screen Back, volume, previous, play/pause, and next buttons.
Swipe up/down to move and swipe left to go back. Since the panel is resistive,
a firm touch or fingernail works better than a light capacitive-style touch.

At 115200 baud, send one serial command per line: `next`, `prev`, `select`,
`back`, `vol+`, `vol-`, `brightness 10..100`, `status`, or `help`. The short
aliases `n`, `p`, and `b` are also accepted. Serial input is non-blocking and
can remain connected while buttons and touch are used.

Bluetooth can also be controlled over serial:

```text
bt scan
bt list
bt connect 1
bt disconnect
```

Run `bt list` a few seconds after scanning; device numbers are one-based.

On startup, the display first shows hardware status, then enters the Kopuz menu.
