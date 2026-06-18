//! SD card mount (FATFS over SPI).
//!
//! The card carries the music. The actual mount lives in the C `storage`
//! component (`storage_mount`), which uses ESP-IDF's SDSPI host/slot macros and
//! mounts FATFS at `/sdcard`. After that, `std::fs` over `/sdcard` just works
//! (that's what `library::scan` relies on). Pins are defined C-side in
//! `components/storage/storage.c`.

use esp_idf_svc::sys as sys;

/// Mount the SD card. Returns Err if no card / bad wiring / not FAT-formatted,
/// but the caller treats that as "empty library" and keeps running.
pub fn mount() -> anyhow::Result<()> {
    let rc = unsafe { sys::storage_mount() };
    if rc != 0 {
        anyhow::bail!("storage_mount failed (rc={rc})");
    }
    Ok(())
}
