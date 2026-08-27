import wave
import math
import struct
import os

def create_test_wav(filepath, duration_sec=5, sample_rate=44100, freq=440.0):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    num_samples = int(duration_sec * sample_rate)
    with wave.open(filepath, 'w') as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2) # 16-bit
        wav.setframerate(sample_rate)

        frames = bytearray()
        for i in range(num_samples):
            t = i / sample_rate
            # 440 Hz stereo with slight panning and envelope
            env = min(1.0, t * 4) * min(1.0, (duration_sec - t) * 4)
            left_val = int(16000 * env * math.sin(2 * math.pi * freq * t))
            right_val = int(16000 * env * math.sin(2 * math.pi * (freq * 1.5) * t))
            frames.extend(struct.pack('<hh', left_val, right_val))

        wav.writeframes(frames)
    print(f"Created {filepath} ({duration_sec}s, {sample_rate}Hz stereo)")

if __name__ == '__main__':
    create_test_wav("sdcard/Kopuz Trio/First Encounter/01 - A440 Test Tone.wav", duration_sec=6, freq=440.0)
    create_test_wav("sdcard/Kopuz Trio/First Encounter/02 - E660 Fifth Harmonic.wav", duration_sec=5, freq=660.0)
    create_test_wav("sdcard/Retro Synth/Distant Stars/01 - C523 Major Tone.wav", duration_sec=7, freq=523.25)
