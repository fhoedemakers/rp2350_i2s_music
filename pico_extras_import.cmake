# This is a copy of <PICO_EXTRAS_PATH>/external/pico_extras_import.cmake

if (DEFINED ENV{PICO_EXTRAS_PATH} AND (NOT PICO_EXTRAS_PATH))
    set(PICO_EXTRAS_PATH $ENV{PICO_EXTRAS_PATH})
    message("Using PICO_EXTRAS_PATH from environment ('${PICO_EXTRAS_PATH}')")
endif ()

if (DEFINED ENV{PICO_EXTRAS_FETCH_FROM_GIT} AND (NOT PICO_EXTRAS_FETCH_FROM_GIT))
    set(PICO_EXTRAS_FETCH_FROM_GIT $ENV{PICO_EXTRAS_FETCH_FROM_GIT})
    message("Using PICO_EXTRAS_FETCH_FROM_GIT from environment ('${PICO_EXTRAS_FETCH_FROM_GIT}')")
endif ()

if (DEFINED ENV{PICO_EXTRAS_FETCH_FROM_GIT_PATH} AND (NOT PICO_EXTRAS_FETCH_FROM_GIT_PATH))
    set(PICO_EXTRAS_FETCH_FROM_GIT_PATH $ENV{PICO_EXTRAS_FETCH_FROM_GIT_PATH})
    message("Using PICO_EXTRAS_FETCH_FROM_GIT_PATH from environment ('${PICO_EXTRAS_FETCH_FROM_GIT_PATH}')")
endif ()

set(PICO_EXTRAS_PATH "${PICO_EXTRAS_PATH}" CACHE PATH "Path to the Raspberry Pi Pico Extras")
set(PICO_EXTRAS_FETCH_FROM_GIT "${PICO_EXTRAS_FETCH_FROM_GIT}" CACHE BOOL "Set to ON to fetch copy of the pico-extras from git if not otherwise locatable")
set(PICO_EXTRAS_FETCH_FROM_GIT_PATH "${PICO_EXTRAS_FETCH_FROM_GIT_PATH}" CACHE FILEPATH "Location to download pico-extras to")

if (NOT PICO_EXTRAS_PATH)
    if (PICO_EXTRAS_FETCH_FROM_GIT)
        include(FetchContent)
        set(FETCHCONTENT_BASE_DIR_SAVE ${FETCHCONTENT_BASE_DIR})
        if (PICO_EXTRAS_FETCH_FROM_GIT_PATH)
            get_filename_component(FETCHCONTENT_BASE_DIR "${PICO_EXTRAS_FETCH_FROM_GIT_PATH}" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
        endif ()
        FetchContent_Declare(
                pico_extras
                GIT_REPOSITORY https://github.com/raspberrypi/pico-extras
                GIT_TAG master
        )
        if (NOT pico_extras)
            message("Downloading Raspberry Pi Pico Extras")
            FetchContent_Populate(pico_extras)
            set(PICO_EXTRAS_PATH ${pico_extras_SOURCE_DIR})
        endif ()
        set(FETCHCONTENT_BASE_DIR ${FETCHCONTENT_BASE_DIR_SAVE})
    else ()
        message(FATAL_ERROR
                "PICO_EXTRAS_PATH is not defined. "
                "Set the PICO_EXTRAS_PATH environment variable, "
                "pass it as a CMake variable (-DPICO_EXTRAS_PATH=...), "
                "or set PICO_EXTRAS_FETCH_FROM_GIT=ON to download it."
        )
    endif ()
endif ()

get_filename_component(PICO_EXTRAS_PATH "${PICO_EXTRAS_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
if (NOT EXISTS ${PICO_EXTRAS_PATH})
    message(FATAL_ERROR "Directory '${PICO_EXTRAS_PATH}' not found")
endif ()

set(PICO_EXTRAS_INIT_CMAKE_FILE ${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
if (NOT EXISTS ${PICO_EXTRAS_INIT_CMAKE_FILE})
    set(PICO_EXTRAS_INIT_CMAKE_FILE ${PICO_EXTRAS_PATH}/pico_extras_init.cmake)
endif ()

if (EXISTS ${PICO_EXTRAS_PATH}/pico_extras_init.cmake)
    include(${PICO_EXTRAS_PATH}/pico_extras_init.cmake)
endif ()

set(PICO_EXTRAS_PATH ${PICO_EXTRAS_PATH} CACHE PATH "Path to the Raspberry Pi Pico Extras" FORCE)
