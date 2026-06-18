//! Decoder boundary. Opens a file and yields S16 PCM blocks.
//!
//! Current impl: raw 16-bit PCM WAV (44.1k stereo) — enough to verify the
//! end-to-end path (SD -> decode -> PWM speaker). For real format support,
//! add `symphonia` (Cargo.toml) and replace `WavDecoder` with a symphonia
//! probe + decode loop; the `Decoder` trait below is the seam.

use super::StreamInfo;
use core::time::Duration;
use std::fs::File;
use std::io::{BufReader, Read};

/// One block of decoded interleaved S16 samples.
pub const BLOCK_FRAMES: usize = 1024;

pub trait Decoder {
    fn info(&self) -> StreamInfo;
    /// Fill `out` with up to BLOCK_FRAMES*channels samples. Returns the number
    /// of samples written; 0 means end-of-stream.
    fn decode_into(&mut self, out: &mut [i16]) -> anyhow::Result<usize>;
    /// Take the embedded cover-art image bytes (JPEG/PNG), if any. Consumed on
    /// first call so it isn't held for the life of the decoder.
    fn cover(&mut self) -> Option<Vec<u8>> {
        None
    }
}

/// Minimal WAV reader: skips the 44-byte canonical header, streams PCM16.
pub struct WavDecoder {
    rdr: BufReader<File>,
    info: StreamInfo,
}

impl WavDecoder {
    pub fn open(path: &str) -> anyhow::Result<Self> {
        let file = File::open(path)?;
        let mut rdr = BufReader::new(file);

        let mut header = [0u8; 44];
        rdr.read_exact(&mut header)?;
        anyhow::ensure!(&header[0..4] == b"RIFF", "not a WAV: {path}");

        let channels = u16::from_le_bytes([header[22], header[23]]) as u8;
        let sample_rate = u32::from_le_bytes([header[24], header[25], header[26], header[27]]);
        let data_len = u32::from_le_bytes([header[40], header[41], header[42], header[43]]);
        let bytes_per_sec = sample_rate * channels as u32 * 2;
        let secs = if bytes_per_sec > 0 {
            data_len / bytes_per_sec
        } else {
            0
        };

        Ok(Self {
            rdr,
            info: StreamInfo {
                sample_rate,
                channels: channels.max(1),
                duration: Duration::from_secs(secs as u64),
            },
        })
    }
}

impl Decoder for WavDecoder {
    fn info(&self) -> StreamInfo {
        self.info
    }

    fn decode_into(&mut self, out: &mut [i16]) -> anyhow::Result<usize> {
        let mut bytes = [0u8; BLOCK_FRAMES * 2 * 2];
        let want = out.len().min(bytes.len() / 2) * 2;
        let n = self.rdr.read(&mut bytes[..want])?;
        let samples = n / 2;
        for i in 0..samples {
            out[i] = i16::from_le_bytes([bytes[i * 2], bytes[i * 2 + 1]]);
        }
        Ok(samples)
    }
}

/// Pick a decoder by extension. Extend as formats are added.
pub fn open(path: &str) -> anyhow::Result<Box<dyn Decoder>> {
    let ext = path.rsplit('.').next().unwrap_or("").to_ascii_lowercase();
    match ext.as_str() {
        "wav" => Ok(Box::new(WavDecoder::open(path)?)),
        "flac" | "mp3" => Ok(Box::new(SymphoniaDecoder::open(path)?)),
        other => anyhow::bail!("no decoder for .{other} yet"),
    }
}

use symphonia::core::audio::SampleBuffer;
use symphonia::core::codecs::{Decoder as SymCodec, DecoderOptions};
use symphonia::core::formats::FormatReader;
use symphonia::core::io::MediaSourceStream;
use symphonia::core::probe::Hint;

pub struct SymphoniaDecoder {
    format: Box<dyn FormatReader>,
    codec: Box<dyn SymCodec>,
    track_id: u32,
    info: StreamInfo,
    sample_buf: Option<SampleBuffer<i16>>,
    buf: Vec<i16>,
    cursor: usize,
    cover: Option<Vec<u8>>,
}

impl SymphoniaDecoder {
    pub fn open(path: &str) -> anyhow::Result<Self> {
        let file = File::open(path)?;
        let mss = MediaSourceStream::new(Box::new(file), Default::default());
        let mut hint = Hint::new();
        if let Some(ext) = std::path::Path::new(path).extension().and_then(|e| e.to_str()) {
            hint.with_extension(ext);
        }
        let probed = symphonia::default::get_probe().format(
            &hint,
            mss,
            &Default::default(),
            &Default::default(),
        )?;
        let mut metadata = probed.metadata;
        let mut format = probed.format;

        // First embedded picture, from the stream metadata (FLAC PICTURE,
        // in-stream ID3 APIC) or the probe metadata (leading ID3v2 on MP3).
        let mut cover = first_visual(&mut format.metadata());
        if cover.is_none() {
            if let Some(m) = metadata.get() {
                cover = first_visual_rev(m.current());
            }
        }

        let track = format
            .default_track()
            .ok_or_else(|| anyhow::anyhow!("no audio track in {path}"))?;
        let params = &track.codec_params;
        let sample_rate = params.sample_rate.unwrap_or(44_100);
        let channels = params.channels.map(|c| c.count()).unwrap_or(2) as u8;
        let duration = match params.n_frames {
            Some(n) => Duration::from_secs_f64(n as f64 / sample_rate as f64),
            None => Duration::ZERO,
        };
        let codec = symphonia::default::get_codecs().make(params, &DecoderOptions::default())?;
        let track_id = track.id;
        Ok(Self {
            format,
            codec,
            track_id,
            info: StreamInfo { sample_rate, channels, duration },
            sample_buf: None,
            buf: Vec::new(),
            cursor: 0,
            cover,
        })
    }
}

fn first_visual(meta: &mut symphonia::core::meta::Metadata) -> Option<Vec<u8>> {
    first_visual_rev(meta.current())
}

fn first_visual_rev(
    rev: Option<&symphonia::core::meta::MetadataRevision>,
) -> Option<Vec<u8>> {
    rev.and_then(|r| r.visuals().first())
        .map(|v| v.data.to_vec())
}

impl Decoder for SymphoniaDecoder {
    fn info(&self) -> StreamInfo {
        self.info
    }

    fn cover(&mut self) -> Option<Vec<u8>> {
        self.cover.take()
    }

    fn decode_into(&mut self, out: &mut [i16]) -> anyhow::Result<usize> {
        let mut written = 0;
        while written < out.len() {
            if self.cursor < self.buf.len() {
                let n = (out.len() - written).min(self.buf.len() - self.cursor);
                out[written..written + n].copy_from_slice(&self.buf[self.cursor..self.cursor + n]);
                self.cursor += n;
                written += n;
                continue;
            }
            self.buf.clear();
            self.cursor = 0;
            let packet = match self.format.next_packet() {
                Ok(p) => p,
                Err(_) => break,
            };
            if packet.track_id() != self.track_id {
                continue;
            }
            match self.codec.decode(&packet) {
                Ok(decoded) => {
                    let spec = *decoded.spec();
                    let cap = decoded.capacity() as u64;
                    let need_new = match &self.sample_buf {
                        Some(sb) => sb.capacity() < (cap as usize) * spec.channels.count(),
                        None => true,
                    };
                    if need_new {
                        self.sample_buf = Some(SampleBuffer::<i16>::new(cap, spec));
                    }
                    let sb = self.sample_buf.as_mut().unwrap();
                    sb.copy_interleaved_ref(decoded);
                    self.buf.extend_from_slice(sb.samples());
                }
                Err(symphonia::core::errors::Error::DecodeError(_)) => continue,
                Err(_) => break,
            }
        }
        Ok(written)
    }
}
