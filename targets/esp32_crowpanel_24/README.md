# CrowPanel 2.4-inch demo (DIS03024H V2.1)

This target boots the real Kopuz interface on the Elecrow 2.4-inch CrowPanel,
scans a FAT32 microSD card, and lets you navigate with the two on-board keys.
It targets the classic `ESP32-WROOM-32-N4`, not an ESP32-S3.

## What works in this first hardware demo

- 320x240 landscape Kopuz interface on the ILI9341V display
- PWM backlight control
- microSD mounting and recursive MP3, FLAC, and WAV library discovery
- two-button navigation
- album-art decoding and display
- conservative, reliable flashing and SD settings

The on-board GPIO26 speaker is deliberately muted in this first build. Tracks
decode and the player UI advances at real time, but proper DAC audio is the next
hardware step. Resistive touch is also reserved for the next pass; the two keys
provide access to every current screen.

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

On startup, the display first shows hardware status, then enters the Kopuz menu.
