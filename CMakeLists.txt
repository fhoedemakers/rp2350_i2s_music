cmake_minimum_required(VERSION 3.13)

# ==============================================================================
# Pico SDK / Pico Extras setup
# ==============================================================================
# Set board type for RP2350 (Pico 2). Override with -DPICO_BOARD=... if needed.
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Pico SDK (must be before project())
include(pico_sdk_import.cmake)

# Pull in Pico Extras (provides pico_audio_i2s)
include(pico_extras_import.cmake)

project(rp2350_i2s_music C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Tell the Pico SDK to scan pico-extras for additional libraries (this is the
# mechanism documented in pico-extras' own README). Must be set BEFORE
# pico_sdk_init() so the SDK picks it up during its CMake configure.
list(APPEND PICO_SDK_POST_LIST_DIRS ${PICO_EXTRAS_PATH})

# Initialize the SDK
pico_sdk_init()

# ==============================================================================
# I2S pin configuration (override on the cmake command line if desired)
#
# Example:
#   cmake -DI2S_DATA_PIN=9 -DI2S_CLOCK_PIN_BASE=10 ..
#
# Note: BCK and LRCK must be on consecutive GPIO pins (LRCK = BCK + 1)
# because pico_audio_i2s uses side-set on two adjacent pins for those signals.
# I2S_CLOCK_PIN_BASE = BCK pin, and LRCK is automatically BCK + 1.
# ==============================================================================
set(I2S_DATA_PIN        9  CACHE STRING "I2S DATA (DIN/SDIN) GPIO pin")
set(I2S_CLOCK_PIN_BASE 10  CACHE STRING "I2S BCK GPIO pin (LRCK will be BCK+1)")

add_subdirectory(src)
