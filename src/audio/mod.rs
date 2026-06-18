//! Audio pipeline (the Rust "high-level" side).
//!
//! Responsibility split:
//!   Rust  — open the file, decode compressed audio to interleaved S16 PCM,
//!           handle seeking/track-end, pace the queue.
//!   C     — `audio_out_*` takes the PCM and drives the speaker (PWM today,
//!           I2S DAC later).
//!
//! The decoder here is a stub boundary: it defines the trait and a passthrough
//! WAV reader so the pipeline links and makes sound. Add symphonia (see
//! Cargo.toml) to decode MP3/FLAC/OGG and fill `decode_into`.

pub mod decoder;

use crate::ffi::audio_out;
use core::time::Duration;

pub const SAMPLE_RATE: u32 = 44_100;
pub const CHANNELS: u8 = 2;

/// Owns the C audio sink lifecycle.
pub struct Audio;

impl Audio {
    pub fn new() -> Self {
        audio_out::init(SAMPLE_RATE, CHANNELS);
        Self
    }

    /// Push one decoded block of interleaved S16 to the speaker.
    pub fn play_block(&self, pcm: &[i16]) {
        audio_out::write(pcm, CHANNELS);
    }

    pub fn pause(&self) {
        audio_out::stop();
    }
}

impl Default for Audio {
    fn default() -> Self {
        Self::new()
    }
}

/// Decoded-track metadata the decoder discovers on open.
#[derive(Debug, Clone, Copy)]
pub struct StreamInfo {
    pub sample_rate: u32,
    pub channels: u8,
    pub duration: Duration,
}
