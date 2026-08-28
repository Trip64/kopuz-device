cmake_minimum_required(VERSION 3.13)

set(NRF52_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main_nrf52.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_nrf_audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_nrf_display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_nrf_storage.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_nrf_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_nrf_power.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_nrf_system.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/audio_player.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/library/library.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ui/framebuffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ui/mini_player.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ui/themes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ui/bsod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ui/qrcodegen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/fonts/font_6x10.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/fonts/font_8x13_bold.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/codecs/decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/codecs/wav_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/codecs/mp3_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/codecs/flac_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/codecs/art_decoder.c
)

add_library(kopuz_nrf52840 STATIC ${NRF52_SRCS})
target_include_directories(kopuz_nrf52840 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/codecs
)
target_compile_definitions(kopuz_nrf52840 PUBLIC TARGET_NRF52840 NRF52840_XXAA)
