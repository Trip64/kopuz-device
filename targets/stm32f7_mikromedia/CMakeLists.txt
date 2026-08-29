cmake_minimum_required(VERSION 3.13)

set(STM32F7_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main_stm32f7.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_stm32_audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_stm32_display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_stm32_storage.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_stm32_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_stm32_power.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hal_stm32_system.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/settings.c
)

add_library(kopuz_stm32f7 STATIC ${STM32F7_SRCS})
target_include_directories(kopuz_stm32f7 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/codecs
)
target_compile_definitions(kopuz_stm32f7 PUBLIC TARGET_STM32F7 TARGET_STM32F7_MIKROMEDIA STM32F746xx)
