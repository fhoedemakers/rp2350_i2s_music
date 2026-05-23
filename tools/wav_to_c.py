#!/usr/bin/env python3
"""
wav_to_c.py -- Convert a PCM WAV file into a C source file containing an
interleaved 16-bit stereo sample array, for embedding in the RP2350 I2S player.

Usage:
    wav_to_c.py <input.wav> <output.c> [--rate HZ]

The output sample rate is whatever the WAV file uses (44100 by default if
--rate is given, simple linear resampling is applied).

Output format:
    - interleaved L, R, L, R, ...
    - int16_t little-endian (host byte order in the C array, the Pico is LE)
    - mono inputs are duplicated to both channels
    - 8-bit unsigned and 24/32-bit signed PCM inputs are converted to s16

Requires only the Python standard library (no numpy/ffmpeg needed).
"""

import argparse
import struct
import sys
import wave
from pathlib import Path


def read_wav(path: Path):
    """Return (sample_rate, num_channels, samples_int16_per_channel_list)."""
    with wave.open(str(path), "rb") as w:
        n_channels = w.getnchannels()
        sample_width = w.getsampwidth()  # bytes per sample
        sample_rate = w.getframerate()
        n_frames = w.getnframes()
        raw = w.readframes(n_frames)

    if n_channels not in (1, 2):
        sys.exit(f"error: only mono/stereo WAV supported (got {n_channels} channels)")

    # Convert to per-channel lists of int16
    if sample_width == 1:
        # 8-bit unsigned PCM -> centre at 128, scale to s16
        samples = struct.unpack(f"<{len(raw)}B", raw)
        samples = [(s - 128) << 8 for s in samples]
    elif sample_width == 2:
        samples = list(struct.unpack(f"<{len(raw) // 2}h", raw))
    elif sample_width == 3:
        # 24-bit signed little-endian, packed
        samples = []
        for i in range(0, len(raw), 3):
            b0, b1, b2 = raw[i], raw[i + 1], raw[i + 2]
            v = b0 | (b1 << 8) | (b2 << 16)
            if v & 0x800000:
                v -= 0x1000000
            samples.append(v >> 8)  # downshift 24->16
    elif sample_width == 4:
        samples = list(struct.unpack(f"<{len(raw) // 4}i", raw))
        samples = [s >> 16 for s in samples]
    else:
        sys.exit(f"error: unsupported sample width {sample_width} bytes")

    # De-interleave into per-channel lists
    if n_channels == 1:
        left = samples
        right = samples
    else:
        left = samples[0::2]
        right = samples[1::2]

    return sample_rate, left, right


def linear_resample(channel, in_rate, out_rate):
    """Crude but adequate linear resampler. Returns new sample list."""
    if in_rate == out_rate:
        return channel
    in_len = len(channel)
    out_len = int(round(in_len * out_rate / in_rate))
    out = [0] * out_len
    ratio = (in_len - 1) / max(out_len - 1, 1)
    for i in range(out_len):
        src = i * ratio
        s0 = int(src)
        s1 = min(s0 + 1, in_len - 1)
        frac = src - s0
        out[i] = int(channel[s0] * (1.0 - frac) + channel[s1] * frac)
    return out


def write_c(path: Path, sample_rate: int, left, right, source_name: str):
    n_frames = min(len(left), len(right))

    with open(path, "w") as f:
        f.write('#include "music_sample.h"\n\n')
        f.write(f"/* Auto-generated from: {source_name}\n")
        f.write(f" * Do not edit by hand; regenerate via the build system.\n")
        f.write(f" *   sample rate : {sample_rate} Hz\n")
        f.write(f" *   channels    : 2 (interleaved L,R)\n")
        f.write(f" *   frames      : {n_frames}\n")
        f.write(f" *   duration    : {n_frames / sample_rate:.2f} s\n")
        f.write(f" */\n\n")
        f.write(f"const uint32_t music_sample_rate       = {sample_rate}u;\n")
        f.write(f"const size_t   music_sample_num_frames = {n_frames};\n\n")
        f.write("const int16_t music_sample_data[] = {\n")

        # 6 frames per line keeps it readable and small
        per_line = 6
        line = []
        for i in range(n_frames):
            l = max(-32768, min(32767, left[i]))
            r = max(-32768, min(32767, right[i]))
            line.append(f"{l},{r}")
            if len(line) == per_line:
                f.write("    " + ", ".join(line) + ",\n")
                line = []
        if line:
            f.write("    " + ", ".join(line) + "\n")
        f.write("};\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path, help="input WAV file")
    ap.add_argument("output", type=Path, help="output C file")
    ap.add_argument("--rate", type=int, default=0,
                    help="target sample rate (Hz); 0 = keep source rate")
    ap.add_argument("--max-seconds", type=float, default=20.0,
                    help="truncate sample to at most this many seconds "
                         "(default: 20.0, set to 0 for no limit)")
    args = ap.parse_args()

    if not args.input.exists():
        sys.exit(f"error: input file not found: {args.input}")

    in_rate, left, right = read_wav(args.input)
    out_rate = args.rate if args.rate > 0 else in_rate

    if out_rate != in_rate:
        print(f"  resampling {in_rate} Hz -> {out_rate} Hz", file=sys.stderr)
        left = linear_resample(left, in_rate, out_rate)
        right = linear_resample(right, in_rate, out_rate)

    # Truncate to at most max_seconds of playback at the output rate.
    # max_seconds <= 0 disables the limit.
    if args.max_seconds > 0:
        max_frames = int(args.max_seconds * out_rate)
        if len(left) > max_frames:
            in_sec = len(left) / out_rate
            print(f"  truncating {in_sec:.2f}s -> {args.max_seconds:.2f}s",
                  file=sys.stderr)
            left  = left[:max_frames]
            right = right[:max_frames]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_c(args.output, out_rate, left, right, args.input.name)

    n = min(len(left), len(right))
    print(f"wav_to_c: {args.input.name} -> {args.output.name} "
          f"({n} frames, {out_rate} Hz, {n / out_rate:.2f}s)", file=sys.stderr)


if __name__ == "__main__":
    main()
