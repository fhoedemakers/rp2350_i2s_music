#ifndef MUSIC_SAMPLE_H
#define MUSIC_SAMPLE_H

#include <stdint.h>
#include <stddef.h>

/*
 * Embedded 16-bit signed stereo PCM music sample.
 *
 * Layout: interleaved L, R, L, R, ...
 * Format: signed 16-bit little-endian
 * Length: music_sample_num_frames frames (one frame = one L + one R pair)
 *
 * All three values are defined in the auto-generated music_sample.c, which
 * is produced from the WAV file by tools/wav_to_c.py during the build.
 */

#define MUSIC_SAMPLE_CHANNELS    2u

extern const uint32_t music_sample_rate;     /* Hz */
extern const int16_t  music_sample_data[];   /* interleaved L,R */
extern const size_t   music_sample_num_frames;

#endif /* MUSIC_SAMPLE_H */
