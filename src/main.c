/*
 * RP2350 I2S music sample player
 *
 * Uses pico_audio_i2s (from pico-extras) to stream a 16-bit stereo PCM
 * sample out over I2S to an external DAC (e.g. PCM5102, UDA1334A, MAX98357).
 *
 * Pin configuration (set in top-level CMakeLists.txt or on the cmake command
 * line via -DI2S_DATA_PIN=... -DI2S_CLOCK_PIN_BASE=...):
 *
 *     DATA  (DIN / SDIN)  -> GPIO PICO_AUDIO_I2S_DATA_PIN        (default 9)
 *     BCK   (BCLK)        -> GPIO PICO_AUDIO_I2S_CLOCK_PIN_BASE  (default 10)
 *     LRCK  (LRCLK / WS)  -> GPIO PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1 (default 11)
 *
 * BCK and LRCK MUST be on consecutive GPIOs; the I2S PIO program uses
 * side-set on those two adjacent pins.
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/audio_i2s.h"

#include "music_sample.h"

/* ---------------------------------------------------------------------------
 * Audio pipeline configuration
 * --------------------------------------------------------------------------- */

/* Number of audio buffers in the producer pool. 3 is the typical sweet spot:
 * one being filled, one queued, one being consumed by the PIO/DMA. */
#define AUDIO_BUFFER_COUNT      3

/* Frames per buffer. A frame = one L+R sample pair for stereo.
 * 1156 frames * 4 bytes/frame = 4624 bytes/buffer, ~26ms at 44.1kHz. */
#define AUDIO_BUFFER_FRAMES     1156

/* ---------------------------------------------------------------------------
 * Audio setup
 * --------------------------------------------------------------------------- */

static audio_buffer_pool_t *init_audio(void) {
    static audio_format_t audio_format = {
        .format        = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = MUSIC_SAMPLE_CHANNELS,
        /* .sample_freq filled in below from the generated sample data */
    };
    audio_format.sample_freq = music_sample_rate;

    static struct audio_buffer_format producer_format = {
        .format        = &audio_format,
        .sample_stride = 4,  /* 2 channels * 2 bytes (S16) */
    };

    audio_buffer_pool_t *producer_pool =
        audio_new_producer_pool(&producer_format,
                                AUDIO_BUFFER_COUNT,
                                AUDIO_BUFFER_FRAMES);

    audio_i2s_config_t config = {
        .data_pin       = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel    = 0,
        .pio_sm         = 0,
    };

    const audio_format_t *output_format =
        audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    bool ok = audio_i2s_connect(producer_pool);
    if (!ok) {
        panic("PicoAudio: audio_i2s_connect failed.\n");
    }

    audio_i2s_set_enabled(true);
    return producer_pool;
}

/* ---------------------------------------------------------------------------
 * Sample streaming
 * --------------------------------------------------------------------------- */

/*
 * Fill one audio buffer with frames from the embedded sample, looping back
 * to the start when we hit the end of the sample.
 *
 * `cursor` is updated in place and tracks the next frame to play.
 */
static void fill_buffer_from_sample(audio_buffer_t *buf, size_t *cursor) {
    int16_t *out = (int16_t *)buf->buffer->bytes;
    const size_t want_frames = buf->max_sample_count;

    size_t produced = 0;
    while (produced < want_frames) {
        size_t available = music_sample_num_frames - *cursor;
        size_t take      = want_frames - produced;
        if (take > available) {
            take = available;
        }

        /* Each frame = 2 int16_t (L, R) in our interleaved sample */
        memcpy(out + produced * 2,
               music_sample_data + *cursor * 2,
               take * 2 * sizeof(int16_t));

        produced += take;
        *cursor  += take;
        if (*cursor >= music_sample_num_frames) {
            *cursor = 0;  /* loop */
        }
    }

    buf->sample_count = (uint32_t)want_frames;
}

/* ---------------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------------- */

int main(void) {
    stdio_init_all();

    /* Brief settle time so any attached serial monitor catches the banner. */
    sleep_ms(500);

    printf("\nRP2350 I2S music player\n");
    printf("  sample rate : %u Hz\n", (unsigned)music_sample_rate);
    printf("  channels    : %u\n",   MUSIC_SAMPLE_CHANNELS);
    printf("  frames      : %u\n",   (unsigned)music_sample_num_frames);
    printf("  duration    : %.2f s\n",
           (double)music_sample_num_frames / (double)music_sample_rate);
    printf("  DATA pin    : GPIO %d\n", PICO_AUDIO_I2S_DATA_PIN);
    printf("  BCK  pin    : GPIO %d\n", PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    printf("  LRCK pin    : GPIO %d\n", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1);

    audio_buffer_pool_t *pool = init_audio();

    size_t cursor = 0;
    for (;;) {
        audio_buffer_t *buf = take_audio_buffer(pool, true);
        fill_buffer_from_sample(buf, &cursor);
        give_audio_buffer(pool, buf);
    }
}
