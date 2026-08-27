import subprocess
import os

# Generate a small 80x80 test PPM image and convert to JPEG
ppm_data = bytearray()
ppm_data.extend(b"P6\n80 80\n255\n")
for y in range(80):
    for x in range(80):
        r = (x * 255) // 80
        g = (y * 255) // 80
        b = ((x + y) * 255) // 160
        ppm_data.extend(bytes([r, g, b]))

with open("sdcard/temp_art.ppm", "wb") as f:
    f.write(ppm_data)

# Convert to JPEG using ffmpeg
subprocess.run(["ffmpeg", "-y", "-i", "sdcard/temp_art.ppm", "sdcard/cover.jpg"], check=True)
os.remove("sdcard/temp_art.ppm")

# 1. Encode MP3 with embedded cover art
subprocess.run([
    "ffmpeg", "-y",
    "-i", "sdcard/Kopuz Trio/First Encounter/01 - A440 Test Tone.wav",
    "-i", "sdcard/cover.jpg",
    "-map", "0:0", "-map", "1:0",
    "-c:a", "libmp3lame", "-b:a", "192k",
    "-c:v", "mjpeg", "-disposition:v", "attached_pic",
    "-id3v2_version", "3",
    "-metadata", "title=Acoustic Resonance",
    "-metadata", "artist=Kopuz Trio",
    "-metadata", "album=First Encounter",
    "sdcard/Kopuz Trio/First Encounter/03 - Acoustic Resonance.mp3"
], check=True)

# 2. Encode FLAC with embedded cover art
subprocess.run([
    "ffmpeg", "-y",
    "-i", "sdcard/Retro Synth/Distant Stars/01 - C523 Major Tone.wav",
    "-i", "sdcard/cover.jpg",
    "-map", "0:0", "-map", "1:0",
    "-c:a", "flac",
    "-c:v", "mjpeg", "-disposition:v", "attached_pic",
    "-metadata", "title=Cyber Pulse",
    "-metadata", "artist=Retro Synth",
    "-metadata", "album=Distant Stars",
    "sdcard/Retro Synth/Distant Stars/02 - Cyber Pulse.flac"
], check=True)

print("Generated test MP3 and FLAC tracks with embedded album art.")
