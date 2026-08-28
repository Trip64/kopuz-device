#!/usr/bin/env python3
"""
Bad Apple Flash Converter - Creates C array for internal flash playback
Generates a 120x68 clip (for 4x scaling) that fits in ~900KB.
"""

import subprocess
import struct
import os
import sys
from pathlib import Path

# Optimized for 4x Scaling (120x68 -> 480x272)
TARGET_WIDTH = 120   
TARGET_HEIGHT = 68   
TARGET_FPS = 12       
MAX_FRAMES = 850     # ~70 seconds
THRESHOLD = 128

def extract_frames(input_video, output_dir, fps, max_frames):
    """Extract frames from video using ffmpeg."""
    os.makedirs(output_dir, exist_ok=True)
    
    cmd = [
        'ffmpeg', '-i', input_video,
        '-vf', f'scale={TARGET_WIDTH}:{TARGET_HEIGHT}:force_original_aspect_ratio=decrease,pad={TARGET_WIDTH}:{TARGET_HEIGHT}:(ow-iw)/2:(oh-ih)/2,format=gray',
        '-r', str(fps),
        '-frames:v', str(max_frames),
        '-f', 'rawvideo',
        '-pix_fmt', 'gray',
        '-y',
        f'{output_dir}/frames.raw'
    ]
    
    print(f"Extracting {max_frames} frames at {TARGET_WIDTH}x{TARGET_HEIGHT}")
    subprocess.run(cmd, check=True, capture_output=True)
    
    return f'{output_dir}/frames.raw'

def rle_encode_frame(frame_data, threshold=128):
    """Convert grayscale frame to RLE-compressed 1-bit data."""
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
    
    if bit_pos < 7:
        bits.append(current_byte)
    
    # RLE encode
    rle = bytearray()
    i = 0
    while i < len(bits):
        value = bits[i]
        count = 1
        while i + count < len(bits) and bits[i + count] == value and count < 255:
            count += 1
        rle.append(count)
        rle.append(value)
        i += count
    
    return bytes(rle)

def convert_to_c_array(input_video, output_header, fps=TARGET_FPS, max_frames=MAX_FRAMES):
    """Convert video to C header."""
    
    temp_dir = Path(output_header).parent / 'temp_flash'
    os.makedirs(temp_dir, exist_ok=True)
    
    # Extract Frames
    raw_file = extract_frames(input_video, str(temp_dir), fps, max_frames)
    frame_size = TARGET_WIDTH * TARGET_HEIGHT
    
    with open(raw_file, 'rb') as f:
        raw_data = f.read()
    
    total_frames = min(len(raw_data) // frame_size, max_frames)
    print(f"Processing {total_frames} frames...")
    
    # Process Video Frames
    all_frames = []
    frame_offsets = []
    current_offset = 0
    
    for i in range(total_frames):
        start = i * frame_size
        frame_data = raw_data[start:start + frame_size]
        rle_data = rle_encode_frame(frame_data, THRESHOLD)
        
        frame_offsets.append(current_offset)
        all_frames.append(rle_data)
        current_offset += len(rle_data) + 4
        
        if i % 100 == 0:
            print(f"  Frame {i}/{total_frames}")
    
    # Write Header
    with open(output_header, 'w') as f:
        f.write("/* Auto-generated Bad Apple video data */\n")
        f.write("#ifndef __BADAPPLE_DATA_H\n")
        f.write("#define __BADAPPLE_DATA_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define VIDEO_WIDTH {TARGET_WIDTH}\n")
        f.write(f"#define VIDEO_HEIGHT {TARGET_HEIGHT}\n")
        f.write(f"#define VIDEO_FPS {fps}\n")
        f.write(f"#define VIDEO_FRAMES {total_frames}\n\n")
        
        # Frame offsets
        f.write("static const uint32_t frame_offsets[] = {\n    ")
        for i, off in enumerate(frame_offsets):
            f.write(f"0x{off:06X}")
            if i < len(frame_offsets) - 1:
                f.write(", ")
            if (i + 1) % 8 == 0:
                f.write("\n    ")
        f.write("\n};\n\n")
        
        # Video data
        f.write("static const uint8_t video_data[] = {\n")
        total_bytes = 0
        for i, frame in enumerate(all_frames):
            size = len(frame)
            f.write(f"    /* F{i} ({size}B) */ ")
            f.write(f"0x{size & 0xFF:02X}, 0x{(size >> 8) & 0xFF:02X}, 0x{(size >> 16) & 0xFF:02X}, 0x{(size >> 24) & 0xFF:02X}, ")
            for j, b in enumerate(frame):
                f.write(f"0x{b:02X}")
                if j < len(frame) - 1:
                    f.write(",")
            f.write(",\n")
            total_bytes += 4 + len(frame)
        f.write("};\n\n")
        f.write("#endif\n")
    
    # Cleanup
    os.remove(raw_file)
    os.rmdir(temp_dir)
    
    print(f"\nConversion complete!")
    print(f"Output: {output_header}")
    print(f"Frames: {total_frames}")
    print(f"Total size: {total_bytes / 1024:.1f} KB")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.mp4> <output.h>")
        sys.exit(1)
    
    convert_to_c_array(sys.argv[1], sys.argv[2])
