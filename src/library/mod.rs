//! Music library: mount the SD card (FATFS) and scan it for playable tracks.
//!
//! The SD card is mounted by ESP-IDF's FATFS VFS, after which it is just a
//! path under `/sdcard`. Tag parsing is left minimal here (filename-derived);
//! wire in a real tag reader later if you want album/artist from metadata.

use crate::app::Track;
use core::time::Duration;
use std::fs;
use std::path::Path;

pub const MOUNT_POINT: &str = "/sdcard";

/// Cap so a runaway scan can't loop forever. High enough for a full card —
/// Track strings live in PSRAM (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL pushes the
/// many small allocs to external RAM), so thousands of tracks fit in the 8 MB.
const MAX_TRACKS: usize = 20000;

const EXTS: &[&str] = &["mp3", "flac"];

/// Recursively scan `root` for audio files.
pub fn scan(root: &str) -> anyhow::Result<Vec<Track>> {
    let mut out = Vec::new();
    scan_dir(Path::new(root), &mut out)?;
    log::info!("library: {} tracks under {}", out.len(), root);
    Ok(out)
}

fn scan_dir(dir: &Path, out: &mut Vec<Track>) -> anyhow::Result<()> {
    let entries = match fs::read_dir(dir) {
        Ok(e) => e,
        Err(e) => {
            log::warn!("scan {:?}: {e}", dir);
            return Ok(());
        }
    };
    for entry in entries.flatten() {
        if out.len() >= MAX_TRACKS {
            log::warn!("library cap {} reached, stopping scan", MAX_TRACKS);
            break;
        }
        let path = entry.path();
        if entry
            .file_name()
            .to_str()
            .map(|n| n.starts_with('.'))
            .unwrap_or(true)
        {
            continue;
        }
        if path.is_dir() {
            scan_dir(&path, out)?;
        } else if is_audio(&path) {
            if let Some(track) = track_from_path(&path) {
                out.push(track);
            }
        }
    }
    Ok(())
}

fn is_audio(path: &Path) -> bool {
    path.extension()
        .and_then(|e| e.to_str())
        .map(|e| EXTS.contains(&e.to_ascii_lowercase().as_str()))
        .unwrap_or(false)
}

fn track_from_path(path: &Path) -> Option<Track> {
    let path_str = path.to_str()?;
    let stem = path.file_stem().and_then(|s| s.to_str()).unwrap_or("?");

    let parent = path.parent();
    let album = parent
        .and_then(dir_name)
        .filter(|n| *n != "sdcard")
        .unwrap_or("Unknown Album");
    let artist = parent
        .and_then(|p| p.parent())
        .and_then(dir_name)
        .filter(|n| *n != "sdcard")
        .unwrap_or("Unknown Artist");

    Some(Track {
        path: path_str.to_owned(),
        title: stem.to_owned(),
        artist: artist.to_owned(),
        album: album.to_owned(),
        duration: Duration::ZERO,
    })
}

fn dir_name(p: &Path) -> Option<&str> {
    p.file_name().and_then(|s| s.to_str())
}
