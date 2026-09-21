# Kopuz Device architecture

## Goals

Kopuz separates portable player behavior from board-specific I/O. The design favors bounded work, streaming media, explicit ownership, and a simulator that can exercise most behavior before hardware is available.

The current architecture is intended to:

- keep the application and UI independent of a vendor SDK;
- decode audio incrementally instead of loading complete tracks;
- use fixed-size display and PCM buffers;
- handle corrupt files and failed allocations without dereferencing invalid memory;
- allow each board to implement storage, display, input, audio, power, timing, and concurrency behind the same interfaces.

It does not currently guarantee a universal RAM ceiling or production readiness on every target.

## Layers

```text
Application state and navigation
            │
            ├── Library and settings
            ├── Framebuffer UI and themes
            └── Audio player orchestration
                         │
                Decoder interface
               ┌─────────┼─────────┐
              WAV       MP3       FLAC
                         │
                 Hardware abstraction
       ┌────────┬────────┬────────┬────────┐
     display   audio    storage   input   system/power
                         │
        simulator / ESP32-S3 / RP2040 / other boards
```

## Application state

`app_state_t` owns the visible player state: queue, grouping, playback order, selections, settings, Bluetooth results, album art, and UI animation data.

Key invariants:

- `current_index` is used only when it is below `queue_len`.
- Every entry in `play_order` refers to an existing queue entry.
- `play_pos` is below `play_order_len` whenever the play order is non-empty.
- A failed allocation leaves the prior valid allocation intact or moves the app to the crash screen.
- Library scans reset playback indices and produce deterministic path ordering.
- Recursive scans are bounded to prevent cyclic or malicious directory trees from exhausting the stack.

`app_init()` establishes these invariants. `app_deinit()` releases queue, grouping, play-order, and album-art ownership for native tests and orderly simulator shutdown.

## Audio pipeline

`audio_player.c` coordinates a decoder with the selected audio HAL:

1. Open the selected track through `decoder_open()`.
2. Validate and expose stream metadata.
3. Configure the audio sink for the decoded sample rate and channel count.
4. Decode bounded PCM blocks into a reusable buffer.
5. Submit samples to the local DAC or Bluetooth output.
6. Update elapsed time and visualizer data.
7. Apply repeat and queue behavior at end of stream.

Failure to open a track or initialize the sink stops playback cleanly. It no longer leaves the state marked as playing with no decoder.

### WAV

The WAV decoder walks RIFF chunks instead of assuming a canonical 44-byte header. It accepts uncompressed PCM with one or two channels, 8–192 kHz sample rates, and 16-, 24-, or 32-bit samples. Unknown and padded chunks are skipped. Data bounds are clamped to the actual file length.

### MP3 and FLAC

MP3 uses the vendored `minimp3` decoder and performs bounded ID3 APIC extraction. FLAC uses the vendored `dr_flac` decoder, supports metadata pictures, and downmixes multichannel streams to stereo.

Real MP3 and FLAC fixture coverage remains a roadmap item. The simulator smoke test runs those codecs when matching fixtures are present and reports them as skipped otherwise.

## Rendering

The UI renders to a 1-bit framebuffer for predictable memory use. Display HALs expand the buffer into their native output format. Color targets can overlay RGB565 album art after the monochrome UI flush.

The simulator exposes multiple display dimensions so layout regressions can be inspected without a board. This is useful validation, but it does not replace testing controller timing, DMA alignment, refresh behavior, or touch coordinates on physical hardware.

## Storage and settings

The storage HAL provides file and directory operations used by the library and decoders. The simulator maps `/sdcard` to a local `sdcard/` directory.

Settings currently support a compact checksummed binary record and an optional text file at `/sdcard/kopuz.cfg`. Persistence still needs to be split into explicit platform backends so ESP32 NVS, MCU flash, and SD-card behavior are not conflated.

## Concurrency

- The simulator runs decoding in an SDL thread.
- ESP32-S3 uses FreeRTOS tasks.
- RP2040 uses the second core for audio work.
- The nRF52840 and STM32F7 scaffolds currently run cooperatively.

Audio resource transitions are protected by the system mutex abstraction. UI state is still shared between the UI and audio loops, so future work should formalize command and telemetry ownership for fully race-free multithreaded builds.

## Memory budgeting

Memory must be measured per target. Significant allocations include:

| Allocation | Approximate size formula |
| --- | --- |
| Track queue | `sizeof(track_t) * MAX_TRACKS` |
| Monochrome framebuffer | `ceil(width / 8) * height` |
| Album art | `ART_BOX_PX * ART_BOX_PX * 2` |
| PCM block | `AUDIO_BUFFER_SAMPLES * sizeof(int32_t)` |
| Decoder state | Codec-dependent |
| Groups and play order | Library-dependent |

On allocation failure, the queue falls back to an eight-track static buffer. The fallback is deliberately small so it does not duplicate the configured full queue in static RAM.

## Validation strategy

The native pipeline is the current quality gate:

- compiler warnings;
- state-machine regression tests;
- deterministic library and grouping tests;
- settings round-trip tests;
- RIFF/WAV parsing and seek tests;
- AddressSanitizer and UndefinedBehaviorSanitizer;
- headless SDL playback smoke test;
- GitHub Actions on every push and pull request.

Hardware support should be promoted from experimental only after its SDK build is reproducible and a physical board passes playback, controls, storage, display, long-run stability, and power tests.
