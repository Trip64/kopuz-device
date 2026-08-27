cmake_minimum_required(VERSION 3.13)

if (NOT PICO_SDK_PATH)
    if (DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    else()
        set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../../pico-sdk")
    endif()
endif()

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(kopuz_rp2040 C CXX ASM)
pico_sdk_init()

add_executable(kopuz_rp2040
    main_rp2040.c
    hal_rp2040_audio.c
    hal_rp2040_display.c
    hal_rp2040_input.c
    hal_rp2040_system.c
    hal_rp2040_power.c
    hal_rp2040_storage.c
    ../../src/app.c
    ../../src/audio_player.c
    ../../src/library/library.c
    ../../src/ui/framebuffer.c
    ../../src/ui/mini_player.c
    ../../src/ui/themes.c
    ../../src/ui/bsod.c
    ../../src/ui/qrcodegen.c
    ../../src/fonts/font_6x10.c
    ../../src/fonts/font_8x13_bold.c
    ../../src/codecs/decoder.c
    ../../src/codecs/wav_decoder.c
    ../../src/codecs/mp3_decoder.c
    ../../src/codecs/flac_decoder.c
    ../../src/codecs/art_decoder.c
)

target_include_directories(kopuz_rp2040 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../..
    ${CMAKE_CURRENT_LIST_DIR}/../../include
    ${CMAKE_CURRENT_LIST_DIR}/../../hal
    ${CMAKE_CURRENT_LIST_DIR}/../../src
    ${CMAKE_CURRENT_LIST_DIR}/../../src/codecs
)

target_compile_options(kopuz_rp2040 PRIVATE
    -Wno-unused-parameter
    -Wno-unused-function
)

target_link_libraries(kopuz_rp2040
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_spi
    hardware_pwm
    hardware_clocks
    hardware_adc
)

pico_add_extra_outputs(kopuz_rp2040)
