#!/usr/bin/env python3
"""
Bad Apple Video Converter for STM32 TFT Display
Converts MP4 video to RLE-compressed 1-bit binary format for SD card playback.

Output format:
- Header: 16 bytes (magic, width, height, frame_count, fps)
- Frames: RLE-compressed 1-bit data

RLE format per frame:
- 4 bytes: frame size
- N bytes: RLE data (count, value pairs where count is 1-127, value is 0x00 or 0xFF)
"""

import subprocess
import struct
import os
import sys
from pathlib import Path

# Configuration
TARGET_WIDTH = 480
TARGET_HEIGHT = 272
TARGET_FPS = 30
THRESHOLD = 128  # B&W threshold

def extract_frames(input_video, output_dir, fps):
    """Extract frames from video using ffmpeg."""
    os.makedirs(output_dir, exist_ok=True)
    
    # Scale to target size and convert to grayscale
    cmd = [
        'ffmpeg', '-i', input_video,
        '-vf', f'scale={TARGET_WIDTH}:{TARGET_HEIGHT}:force_original_aspect_ratio=decrease,pad={TARGET_WIDTH}:{TARGET_HEIGHT}:(ow-iw)/2:(oh-ih)/2,format=gray',
        '-r', str(fps),
        '-y',
        f'{output_dir}/frame_%05d.raw'
    ]
    
    # Use rawvideo output for simplicity
    cmd = [
        'ffmpeg', '-i', input_video,
        '-vf', f'scale={TARGET_WIDTH}:{TARGET_HEIGHT}:force_original_aspect_ratio=decrease,pad={TARGET_WIDTH}:{TARGET_HEIGHT}:(ow-iw)/2:(oh-ih)/2,format=gray',
        '-r', str(fps),
        '-f', 'rawvideo',
        '-pix_fmt', 'gray',
        '-y',
        f'{output_dir}/frames.raw'
    ]
    
    print(f"Extracting frames: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)
    
    return f'{output_dir}/frames.raw'

def rle_encode_frame(frame_data, threshold=128):
    """Convert grayscale frame to RLE-compressed 1-bit data."""
    # Convert to 1-bit
    bits = bytearray()
    current_byte = 0
    bit_pos = 7
    
    for pixel in frame_data:
        if pixel >= threshold:
            current_byte |= (1 << bit_pos)
        bit_pos -= 1
        if bit_pos < 0:
            bits.append(current_byte)
            current_byte = 0
            bit_pos = 7
    
    # Handle remaining bits
    if bit_pos < 7:
        bits.append(current_byte)
    
    # RLE encode the 1-bit data
    rle = bytearray()
    i = 0
    while i < len(bits):
        value = bits[i]
        count = 1
        
        # Count consecutive identical bytes (max 255)
        while i + count < len(bits) and bits[i + count] == value and count < 255:
            count += 1
        
        rle.append(count)
        rle.append(value)
        i += count
    
    return bytes(rle)

def convert_video(input_video, output_file, fps=TARGET_FPS):
    """Convert video to custom binary format."""
    
    # Create temp directory
    temp_dir = Path(output_file).parent / 'temp_frames'
    os.makedirs(temp_dir, exist_ok=True)
    
    # Extract all frames as raw grayscale
    raw_file = extract_frames(input_video, str(temp_dir), fps)
    
    # Calculate frame size
    frame_size = TARGET_WIDTH * TARGET_HEIGHT
    
    # Read raw frames data
    with open(raw_file, 'rb') as f:
        raw_data = f.read()
    
    total_frames = len(raw_data) // frame_size
    print(f"Total frames: {total_frames}")
    
    # Process frames and write output
    with open(output_file, 'wb') as out:
        # Write header
        # Magic: "BAPV" (Bad Apple Video)
        out.write(b'BAPV')
        out.write(struct.pack('<H', TARGET_WIDTH))
        out.write(struct.pack('<H', TARGET_HEIGHT))
        out.write(struct.pack('<I', total_frames))
        out.write(struct.pack('<H', fps))
        out.write(b'\x00\x00')  # Padding to 16 bytes
        
        frame_offsets = []
        data_start = 16 + total_frames * 4  # Header + offset table
        
        # Reserve space for offset table
        out.write(b'\x00' * (total_frames * 4))
        
        # Process each frame
        for i in range(total_frames):
            if i % 100 == 0:
                print(f"Processing frame {i}/{total_frames}")
            
            start = i * frame_size
            end = start + frame_size
            frame_data = raw_data[start:end]
            
            # RLE encode
            rle_data = rle_encode_frame(frame_data, THRESHOLD)
            
            # Record offset and write frame
            frame_offsets.append(out.tell())
            
            # Write frame size then data
            out.write(struct.pack('<I', len(rle_data)))
            out.write(rle_data)
        
        # Go back and write offset table
        out.seek(16)
        for offset in frame_offsets:
            out.write(struct.pack('<I', offset))
    
    # Cleanup
    os.remove(raw_file)
    os.rmdir(temp_dir)
    
    # Report stats
    output_size = os.path.getsize(output_file)
    print(f"\nConversion complete!")
    print(f"Output: {output_file}")
    print(f"Size: {output_size / 1024 / 1024:.2f} MB")
    print(f"Frames: {total_frames}")
    print(f"Avg bytes/frame: {output_size / total_frames:.0f}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.mp4> <output.bin>")
        sys.exit(1)
    
    convert_video(sys.argv[1], sys.argv[2])
