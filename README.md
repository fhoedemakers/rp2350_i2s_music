# RP2350 I2S Music Player

A complete, ready-to-build C project for the **Raspberry Pi Pico 2 (RP2350)**
that plays an embedded music sample out over **I2S** to an external DAC,
using [`pico_audio_i2s`](https://github.com/raspberrypi/pico-extras/tree/master/src/rp2_common/pico_audio_i2s)
from `pico-extras`.

The music sample is embedded directly into the firmware at **build time**:
drop a `.wav` file into `assets/`, and CMake regenerates the C sample array
automatically on every build. The DAC GPIO pins, sample rate, and maximum
sample length are all configurable.

---

## Table of contents

1. [Quick start](#quick-start)
2. [Prerequisites](#prerequisites)
3. [Project layout](#project-layout)
4. [Building](#building)
5. [Configuration options](#configuration-options)
   - [I2S pins](#i2s-pins)
   - [Music WAV file](#music-wav-file)
   - [Sample rate](#sample-rate)
   - [Maximum duration](#maximum-duration)
6. [Hardware wiring](#hardware-wiring)
7. [Flashing the Pico 2](#flashing-the-pico-2)
8. [Runtime behaviour](#runtime-behaviour)
9. [How it works internally](#how-it-works-internally)
10. [Troubleshooting](#troubleshooting)
11. [Flash budget reference](#flash-budget-reference)

---

## Quick start

If you already have the Pico SDK and pico-extras set up:

```sh
# 1. Put your music file in place
cp ~/Music/my_song.wav assets/music.wav

# 2. Configure with your DAC pins (DATA=26, BCK=27, LRCK=28 are the defaults)
mkdir build && cd build
cmake -DI2S_DATA_PIN=26 -DI2S_CLOCK_PIN_BASE=27 ..

# 3. Build
make -j

# 4. Flash build/src/music_player.uf2 to your Pico 2 in BOOTSEL mode
```

That's it — the Pico 2 will boot, print a banner over USB serial, and start
looping your music sample.

---

## Prerequisites

You need all of these installed on your build machine:

| Tool | Minimum version | Purpose |
|------|-----------------|---------|
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) | 2.0.0 | RP2350 support |
| [pico-extras](https://github.com/raspberrypi/pico-extras) | latest `master` | provides `pico_audio_i2s` |
| `arm-none-eabi-gcc` | 13.x | cross-compiler |
| CMake | 3.13 | build system |
| Python 3 | 3.6+ | runs the WAV-to-C converter |

The Pico SDK already requires Python 3 internally, so you almost certainly
have it. **No `ffmpeg` or `numpy` is needed** — the converter uses only the
Python standard library.

Set these environment variables (typically in your shell's rc file):

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras
```

Verify with:
```sh
ls $PICO_SDK_PATH/pico_sdk_init.cmake
ls $PICO_EXTRAS_PATH/src/rp2_common/pico_audio_i2s/include/pico/audio_i2s.h
```

Both files must exist.

---

## Project layout

```
rp2350_i2s_music/
├── CMakeLists.txt              Top-level: SDK init, board type, I2S pins
├── pico_sdk_import.cmake       Standard Pico SDK importer
├── pico_extras_import.cmake    Standard pico-extras importer
├── README.md                   This file
├── assets/
│   └── music.wav               Drop your WAV here (or override via cmake)
├── tools/
│   └── wav_to_c.py             WAV -> C converter (run by CMake at build)
└── src/
    ├── CMakeLists.txt          Generation rule + executable target
    ├── main.c                  Audio init + producer streaming loop
    └── music_sample.h          extern declarations for the sample data
                                  (music_sample.c is generated at build time
                                   into the build directory)
```

You will typically only ever edit:
- `assets/music.wav` — to change the song
- `CMakeLists.txt` — to change the default pin assignment

You should not need to edit `main.c` for normal use.

---

## Building

### First build

```sh
mkdir build
cd build
cmake ..
make -j
```

CMake will:
1. Locate the Pico SDK and pico-extras (via your environment variables)
2. Set up the cross-compiler for `rp2350-arm-s`
3. Run the WAV-to-C converter on `assets/music.wav` to generate
   `build/src/music_sample.c`
4. Compile and link everything into `build/src/music_player.uf2`

Expected build artifact: **`build/src/music_player.uf2`** (~50–80 KB code
plus your sample data).

### Rebuilding

For incremental rebuilds, just `make -j` again. CMake automatically
re-runs the WAV converter if the source WAV file *or* the script changes.

### Forcing a full reconfigure

If you change a CMake option, the safest thing is to wipe the build dir:

```sh
rm -rf build && mkdir build && cd build && cmake [options] ..
make -j
```

Why: CMake caches your previous option values in `CMakeCache.txt`. Editing
the default in `CMakeLists.txt` won't override a cached value, so when in
doubt, blow away the build dir.

---

## Configuration options

All of the following can be set on the cmake command line with `-D` flags.
They can also be edited as defaults in `CMakeLists.txt` (top-level for the
pin options, `src/CMakeLists.txt` for the sample-related ones).

### I2S pins

| Variable               | Default | Description                              |
|------------------------|---------|------------------------------------------|
| `I2S_DATA_PIN`         | `26`    | GPIO carrying I2S DATA (DIN/SDIN)        |
| `I2S_CLOCK_PIN_BASE`   | `27`    | GPIO carrying I2S BCK; LRCK = this + 1   |

**Important constraint:** BCK and LRCK *must* be on consecutive GPIOs.
The PIO program uses side-set across two adjacent pins to generate them.
You only set the base (BCK) pin; LRCK is implicitly `BCK + 1`.

Example — DATA on GPIO 9, BCK on GPIO 10, LRCK implicitly on GPIO 11:
```sh
cmake -DI2S_DATA_PIN=9 -DI2S_CLOCK_PIN_BASE=10 ..
```

These values flow into the C code via `-D` preprocessor flags
(`PICO_AUDIO_I2S_DATA_PIN`, `PICO_AUDIO_I2S_CLOCK_PIN_BASE`) which the
`audio_i2s_config_t` struct in `main.c` reads.

### Music WAV file

| Variable          | Default                | Description                            |
|-------------------|------------------------|----------------------------------------|
| `MUSIC_WAV_FILE`  | `assets/music.wav`     | Path to the WAV file to embed          |

```sh
cmake -DMUSIC_WAV_FILE=/absolute/path/to/song.wav ..
```

Supported WAV formats:
- Mono or stereo
- 8-bit unsigned, 16-bit signed, 24-bit signed, or 32-bit signed PCM
- Any sample rate

Mono files are automatically duplicated to both stereo channels. The
output is always 16-bit signed stereo (which is what `pico_audio_i2s`
streams natively).

> **Note:** Only standard uncompressed PCM WAV is supported. ADPCM or
> MP3-in-WAV will fail. Convert with ffmpeg first if needed:
> ```sh
> ffmpeg -i source.mp3 -acodec pcm_s16le -ar 44100 -ac 2 assets/music.wav
> ```

### Sample rate

| Variable              | Default | Description                                  |
|-----------------------|---------|----------------------------------------------|
| `MUSIC_SAMPLE_RATE_HZ`| `0`     | Force target rate in Hz; `0` keeps WAV's own |

```sh
cmake -DMUSIC_SAMPLE_RATE_HZ=22050 ..
```

When `0` (default), whatever rate the source WAV uses is preserved
exactly — no resampling. When set, the script linearly resamples to that
rate before embedding.

Common values:
- `44100` — CD quality
- `22050` — half the data; usable for voice and many music samples
- `48000` — video standard

Lowering the rate is the easiest way to fit more audio in flash.

> **Quality note:** the built-in resampler is simple linear interpolation —
> fine for most music samples and very fast, but not audiophile-grade.
> For pristine quality, pre-resample with `sox` or `ffmpeg` and leave
> `MUSIC_SAMPLE_RATE_HZ=0`.

### Maximum duration

| Variable             | Default | Description                                |
|----------------------|---------|--------------------------------------------|
| `MUSIC_MAX_SECONDS`  | `20`    | Cap embedded sample length (`0` = no cap)  |

```sh
cmake -DMUSIC_MAX_SECONDS=10 ..   # truncate at 10s
cmake -DMUSIC_MAX_SECONDS=0  ..   # no truncation (use entire WAV)
```

The cap is applied *after* resampling, so it always means "this many
seconds of actual playback." This is the main knob for fitting larger
WAVs into the Pico 2's 4 MB flash — see [Flash budget reference](#flash-budget-reference)
below.

### Combining options

All options stack:

```sh
cmake \
  -DMUSIC_WAV_FILE=~/Music/podcast.wav \
  -DMUSIC_SAMPLE_RATE_HZ=22050 \
  -DMUSIC_MAX_SECONDS=45 \
  -DI2S_DATA_PIN=26 \
  -DI2S_CLOCK_PIN_BASE=27 \
  ..
```

---

## Hardware wiring

You need an external I2S DAC. Common, cheap, and known-compatible:

- **PCM5102 / PCM5102A** breakout (great quality, 3.3V, line out)
- **UDA1334A** breakout (Adafruit; line/headphone out)
- **MAX98357A** breakout (Adafruit/SparkFun; mono Class-D amp, drives a small speaker directly)

### Default pinout (DATA=26, BCK=27, LRCK=28)

| Pico 2 GPIO | I2S signal | DAC pin name(s)         |
|-------------|------------|-------------------------|
| GPIO 26     | DATA       | DIN / SDIN / SDATA      |
| GPIO 27     | BCK        | BCK / BCLK              |
| GPIO 28     | LRCK       | LRCK / LRCLK / WS       |
| GND         | GND        | GND                     |
| 3V3 or 5V   | VCC        | per DAC datasheet       |

### DAC-specific notes

**PCM5102:** also tie these to set the DAC's mode:
- `SCK` → GND (uses internal PLL — required for use with the Pico)
- `FLT` → GND (normal filter response)
- `DEMP` → GND (de-emphasis off)
- `XSMT` → 3.3V (un-mute)
- `FMT` → GND (I2S format, not left-justified)

**UDA1334A:** all the input pins default appropriately; just wire VIN, GND,
BCLK, WSEL, DIN.

**MAX98357A:** wire DIN, BCLK, LRCLK as shown. Tie SD_MODE per the table on
the breakout silkscreen to select mono mix vs. left only vs. right only.

### Powering the Pico 2 from USB while the DAC runs

Pico 2 supplies 3V3 on its 3V3(OUT) pin which can drive a small DAC breakout
directly. If you're driving a speaker through a Class-D amp like the
MAX98357A, power that from 5V (VBUS) instead — Class-D amps draw current
spikes that can brown out the 3V3 rail.

---

## Flashing the Pico 2

1. Hold the **BOOTSEL** button on the Pico 2.
2. While holding it, plug the Pico 2 into your computer via USB.
3. Release BOOTSEL. The Pico 2 enumerates as a USB mass-storage device named
   `RP2350`.
4. Drag and drop `build/src/music_player.uf2` onto that drive.
5. The Pico 2 will reboot automatically into the new firmware.

Alternative (using `picotool`):

```sh
picotool load -fx build/src/music_player.uf2
```

---

## Runtime behaviour

After flashing, the Pico 2 enumerates as a USB serial device. Connect with
your favourite terminal (`screen /dev/ttyACM0 115200`, `minicom`, PuTTY,
the Arduino IDE serial monitor, etc.) and you'll see:

```
RP2350 I2S music player
  sample rate : 44100 Hz
  channels    : 2
  frames      : 882000
  duration    : 20.00 s
  DATA pin    : GPIO 26
  BCK  pin    : GPIO 27
  LRCK pin    : GPIO 28
```

After this banner, the sample begins playing and loops continuously. There
is no UI — playback is automatic and continuous.

---

## How it works internally

### The build pipeline

```
   assets/music.wav
         |
         |  CMake add_custom_command runs at build time
         |  ─→ python3 tools/wav_to_c.py
         |        (parses WAV header, decodes PCM,
         |         optionally resamples, truncates,
         |         emits C array)
         v
   build/src/music_sample.c       ←  generated, NOT in your source tree
         |
         |  compiled together with main.c
         v
   build/src/music_player.uf2     ←  flash this
```

CMake tracks the WAV and the script as dependencies of the generated `.c`
file, so any change to either retriggers the conversion automatically.
The generated C file lives in the build directory only — your source tree
stays clean.

### The audio pipeline at runtime

```
   music_sample_data[]                       (in flash, ~3.4 MB at 20s/44.1kHz)
         |
         |  memcpy chunks of frames
         v
   producer_pool   ←─ 3 audio_buffers, ~1156 frames each
         |
         |  give_audio_buffer()  →  PIO/DMA consumer
         v
   PIO state machine generates BCK + LRCK via side-set,
   DMA feeds DATA from buffers byte-by-byte
         |
         v
   I2S DAC → analog audio
```

The CPU's job is just keeping the buffer pool fed. `take_audio_buffer()`
blocks when the pool is full, so the loop in `main.c` naturally
self-throttles to the playback rate — no timing logic needed.

### Why three buffers?

Standard audio pipelining: one buffer being filled by the CPU, one queued
for DMA, one being consumed by the PIO. This hides any short jitter in
the producer thread. At 44.1 kHz / 1156 frames per buffer, each buffer is
~26 ms of audio, giving ~78 ms of total cushion.

---

## Troubleshooting

### `fatal error: pico/audio_i2s.h: No such file or directory`

The pico-extras library targets haven't been added to the build. Confirm
your top-level `CMakeLists.txt` has this line *before* `pico_sdk_init()`:

```cmake
list(APPEND PICO_SDK_POST_LIST_DIRS ${PICO_EXTRAS_PATH})
```

This is the official mechanism (documented in pico-extras' README) that
tells the SDK to scan pico-extras for additional libraries during init.

Then wipe and rebuild:
```sh
rm -rf build/* && cmake .. && make -j
```

### `error: Music WAV file not found`

CMake didn't find a WAV at the configured location. Either put one at
`assets/music.wav`, or pass `-DMUSIC_WAV_FILE=/path/to/file.wav`.

### `error: only mono/stereo WAV supported` or `error: unsupported sample width`

Your WAV uses a format the script doesn't handle (e.g. >2 channels,
floating-point samples, or compressed PCM). Convert it first:

```sh
ffmpeg -i source.wav -acodec pcm_s16le -ar 44100 -ac 2 assets/music.wav
```

### Build error: `region 'FLASH' overflowed`

Your sample is too big to fit alongside the firmware. Options in order of
preference:
1. Reduce `MUSIC_MAX_SECONDS`
2. Lower `MUSIC_SAMPLE_RATE_HZ` to 22050 (halves the data)
3. Both of the above

See [Flash budget reference](#flash-budget-reference).

### No audio comes out of the DAC, but the banner prints fine over serial

Check, in this order:
1. **Wiring**: DATA, BCK, LRCK, GND, VCC. Easy to swap BCK↔LRCK.
2. **Pin numbers**: the boot banner shows what the firmware *thinks* it's
   using. Make sure those match your wiring.
3. **DAC mode pins** (PCM5102): SCK → GND is essential.
4. **DAC power**: a multimeter on the DAC's VCC pin should read what the
   datasheet expects (3.3V or 5V).
5. **Probe with a scope or logic analyser** on BCK — you should see a
   continuous clock at ~2.8 MHz (= 44100 × 64). No clock → I2S isn't
   running; check pins.

### Audio plays at the wrong pitch / speed

The runtime sample rate doesn't match the source. The generated banner
shows the actual sample rate; if your WAV was 48 kHz but you forced
`MUSIC_SAMPLE_RATE_HZ=44100`, the linear resampler will have already
handled it. If the source WAV claims a non-standard rate that's
incompatible with your DAC, fall back to forcing 44100.

### Audio is glitchy or has clicks

Most likely your sample data isn't `int16_t` aligned, which shouldn't
happen with this script. If you replaced `music_sample_data` by hand,
double-check that array. Otherwise, try `MUSIC_SAMPLE_RATE_HZ=44100`
explicitly — some DACs are happier at standard rates.

### `make` does not regenerate `music_sample.c` after I changed the WAV

It should, automatically. If it doesn't (rare — usually a filesystem
timestamp issue), `touch tools/wav_to_c.py` to bump the dependency, or
just `rm build/src/music_sample.c && make`.

---

## Flash budget reference

The Pico 2 has **4 MB of flash**. Subtract roughly 100–200 KB for code,
bootloader, and ROM tables; you have roughly **3.8 MB** for audio data.

Bytes per second for 16-bit stereo PCM:

| Sample rate | Bytes/sec    | ~1 second  | ~10 seconds  | ~20 seconds   |
|-------------|--------------|-----------|---------------|---------------|
| 8 000       |  32 000      | 32 KB      |  320 KB       |  640 KB       |
| 22 050      |  88 200      | 88 KB      |  880 KB       |  1.7 MB       |
| 44 100      | 176 400      | 176 KB     |  1.7 MB       |  3.4 MB       |
| 48 000      | 192 000      | 192 KB     |  1.9 MB       |  3.7 MB       |

At 44.1 kHz stereo, **20 seconds is roughly the practical limit** before
you start running out of room for code. If you need more, drop to 22 kHz
or use the `MUSIC_MAX_SECONDS` cap.

For dramatically longer audio you'd need to stream from external storage
(SD card / external flash) rather than embedding — that's outside the
scope of this project.

---

## License & credits

`pico_audio_i2s` is part of Raspberry Pi's `pico-extras` and is BSD-3-Clause
licensed. The rest of this project is intentionally trivially licenseable
— treat it as public domain / CC0 unless your organization needs something
specific.
