# ==============================================================================
# Music sample generation
#
# The C file containing the embedded PCM sample is generated at build time
# from a .wav file via tools/wav_to_c.py. The build re-runs the converter
# whenever the WAV or the script changes.
#
# The WAV file path can be overridden on the cmake command line, e.g.
#   cmake -DMUSIC_WAV_FILE=/path/to/my/song.wav ..
# Default: assets/music.wav in the project root.
#
# Optional: force a target sample rate (Hz). Set MUSIC_SAMPLE_RATE_HZ to 0
# (the default) to keep whatever rate the WAV already has.
# ==============================================================================
set(MUSIC_WAV_FILE        "${CMAKE_SOURCE_DIR}/assets/music.wav"
    CACHE FILEPATH "Path to the WAV file to embed as the music sample")
set(MUSIC_SAMPLE_RATE_HZ  0
    CACHE STRING   "Target sample rate in Hz, 0 = use the WAV's own rate")
set(MUSIC_MAX_SECONDS     20
    CACHE STRING   "Truncate the embedded sample to at most this many seconds (0 = no limit)")

set(MUSIC_SAMPLE_GEN_C "${CMAKE_CURRENT_BINARY_DIR}/music_sample.c")
set(WAV_TO_C_SCRIPT    "${CMAKE_SOURCE_DIR}/tools/wav_to_c.py")

if(NOT EXISTS "${MUSIC_WAV_FILE}")
    message(FATAL_ERROR
        "Music WAV file not found: ${MUSIC_WAV_FILE}\n"
        "Place a WAV file at that location, or override with "
        "-DMUSIC_WAV_FILE=/path/to/your.wav")
endif()

# Forward --rate only when a non-zero value was given
if(MUSIC_SAMPLE_RATE_HZ GREATER 0)
    set(_rate_args --rate ${MUSIC_SAMPLE_RATE_HZ})
else()
    set(_rate_args "")
endif()

add_custom_command(
    OUTPUT  "${MUSIC_SAMPLE_GEN_C}"
    COMMAND ${Python3_EXECUTABLE}
            "${WAV_TO_C_SCRIPT}"
            "${MUSIC_WAV_FILE}"
            "${MUSIC_SAMPLE_GEN_C}"
            --max-seconds ${MUSIC_MAX_SECONDS}
            ${_rate_args}
    DEPENDS "${MUSIC_WAV_FILE}" "${WAV_TO_C_SCRIPT}"
    COMMENT "Embedding ${MUSIC_WAV_FILE} -> music_sample.c (max ${MUSIC_MAX_SECONDS}s)"
    VERBATIM
)

# A phony target some IDEs like to display the custom command under
add_custom_target(generate_music_sample DEPENDS "${MUSIC_SAMPLE_GEN_C}")

# ==============================================================================
# Music player executable
# ==============================================================================
add_executable(music_player
    main.c
    "${MUSIC_SAMPLE_GEN_C}"
)

# The generated .c includes "music_sample.h" from this directory
target_include_directories(music_player PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# Make the I2S pin configuration available to the C code
target_compile_definitions(music_player PRIVATE
    PICO_AUDIO_I2S_DATA_PIN=${I2S_DATA_PIN}
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=${I2S_CLOCK_PIN_BASE}
)

target_link_libraries(music_player PRIVATE
    pico_stdlib
    pico_audio_i2s
)

# Enable USB stdio for logging; disable UART so we don't conflict with the
# default UART pins.
pico_enable_stdio_usb(music_player  1)
pico_enable_stdio_uart(music_player 0)

# Create map/bin/hex/uf2 in addition to ELF
pico_add_extra_outputs(music_player)
