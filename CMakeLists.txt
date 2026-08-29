cmake_minimum_required(VERSION 3.15)
project(kopuz_device C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

# Compilation Options
set(KOPUZ_TARGET "SIMULATOR" CACHE STRING "Target hardware: SIMULATOR, RP2040, or ESP32S3")

if(KOPUZ_TARGET STREQUAL "SIMULATOR")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        include
        hal
        src
        src/codecs
        ${SDL2_INCLUDE_DIRS}
    )

    set(CORE_SOURCES
        src/app.c
        src/audio_player.c
        src/library/library.c
        src/ui/framebuffer.c
        src/ui/mini_player.c
        src/ui/themes.c
        src/ui/bsod.c
        src/ui/qrcodegen.c
        src/fonts/font_6x10.c
        src/fonts/font_8x13_bold.c
        src/codecs/decoder.c
        src/codecs/wav_decoder.c
        src/codecs/mp3_decoder.c
        src/codecs/flac_decoder.c
        src/codecs/art_decoder.c
        src/settings.c
    )

    set(SIM_SOURCES
        targets/simulator/main_sim.c
        targets/simulator/hal_sim_audio.c
        targets/simulator/hal_sim_display.c
        targets/simulator/hal_sim_input.c
        targets/simulator/hal_sim_power.c
        targets/simulator/hal_sim_storage.c
        targets/simulator/hal_sim_system.c
    )

    add_executable(kopuz_sim
        ${CORE_SOURCES}
        ${SIM_SOURCES}
    )

    target_compile_definitions(kopuz_sim PRIVATE
        TARGET_SIMULATOR=1
    )

    target_compile_options(kopuz_sim PRIVATE
        -O3
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-unused-function
    )

    target_link_directories(kopuz_sim PRIVATE
        ${SDL2_LIBRARY_DIRS}
        /opt/homebrew/lib
    )

    target_link_libraries(kopuz_sim PRIVATE
        ${SDL2_LIBRARIES}
        m
    )

    message(STATUS "Configured Kopuz Device Simulator target: kopuz_sim")

elseif(KOPUZ_TARGET STREQUAL "RP2040" OR KOPUZ_TARGET STREQUAL "RP2350")
    add_subdirectory(targets/rp2040)
endif()
