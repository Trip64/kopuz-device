cmake_minimum_required(VERSION 3.15)
project(kopuz_device C CXX)

include(CTest)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compilation Options
set(KOPUZ_TARGET "SIMULATOR" CACHE STRING "Target hardware: SIMULATOR, RP2040, or ESP32S3")
option(KOPUZ_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(kopuz_enable_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-function
    )
endfunction()

function(kopuz_enable_sanitizers target)
    if(KOPUZ_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

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
        targets/simulator/hal_sim_ble.c
    )

    add_executable(kopuz_sim
        ${CORE_SOURCES}
        ${SIM_SOURCES}
    )

    target_compile_definitions(kopuz_sim PRIVATE
        TARGET_SIMULATOR=1
        KOPUZ_SIMULATOR=1
    )

    kopuz_enable_warnings(kopuz_sim)
    kopuz_enable_sanitizers(kopuz_sim)

    target_link_directories(kopuz_sim PRIVATE ${SDL2_LIBRARY_DIRS})

    target_link_libraries(kopuz_sim PRIVATE
        ${SDL2_LIBRARIES}
        m
    )

    message(STATUS "Configured Kopuz Device Simulator target: kopuz_sim")

    if(BUILD_TESTING)
        add_executable(kopuz_app_tests
            tests/test_app.c
            src/app.c
            src/ui/themes.c
        )
        target_include_directories(kopuz_app_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            include
            hal
            src
        )
        target_compile_definitions(kopuz_app_tests PRIVATE TARGET_SIMULATOR=1)
        kopuz_enable_warnings(kopuz_app_tests)
        kopuz_enable_sanitizers(kopuz_app_tests)
        add_test(NAME app_state COMMAND kopuz_app_tests)

        add_executable(kopuz_wav_tests
            tests/test_wav_decoder.c
            src/codecs/decoder.c
            src/codecs/wav_decoder.c
        )
        target_include_directories(kopuz_wav_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            include
            hal
            src
            src/codecs
        )
        kopuz_enable_warnings(kopuz_wav_tests)
        kopuz_enable_sanitizers(kopuz_wav_tests)
        add_test(NAME wav_decoder COMMAND kopuz_wav_tests)

        add_executable(kopuz_stream_decoder_tests
            tests/test_stream_decoders.c
            src/codecs/decoder.c
            src/codecs/mp3_decoder.c
            src/codecs/flac_decoder.c
            targets/simulator/hal_sim_storage.c
        )
        target_include_directories(kopuz_stream_decoder_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            include
            hal
            src
            src/codecs
        )
        kopuz_enable_warnings(kopuz_stream_decoder_tests)
        kopuz_enable_sanitizers(kopuz_stream_decoder_tests)
        add_test(NAME malformed_stream_decoders COMMAND kopuz_stream_decoder_tests)

        add_executable(kopuz_library_tests
            tests/test_library.c
            src/app.c
            src/library/library.c
            src/ui/themes.c
            targets/simulator/hal_sim_storage.c
        )
        target_include_directories(kopuz_library_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            include
            hal
            src
        )
        kopuz_enable_warnings(kopuz_library_tests)
        kopuz_enable_sanitizers(kopuz_library_tests)
        add_test(NAME library_scan COMMAND kopuz_library_tests)

        add_executable(kopuz_settings_tests
            tests/test_settings.c
            src/settings.c
            src/ui/themes.c
        )
        target_include_directories(kopuz_settings_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            include
            hal
            src
        )
        kopuz_enable_warnings(kopuz_settings_tests)
        kopuz_enable_sanitizers(kopuz_settings_tests)
        add_test(NAME settings_round_trip COMMAND kopuz_settings_tests)
    endif()

elseif(KOPUZ_TARGET STREQUAL "RP2040" OR KOPUZ_TARGET STREQUAL "RP2350")
    add_subdirectory(targets/rp2040)
else()
    message(FATAL_ERROR
        "Unsupported KOPUZ_TARGET='${KOPUZ_TARGET}'. Use SIMULATOR, RP2040, or RP2350. "
        "ESP32-S3 builds are driven by ESP-IDF from targets/esp32s3_tdisplay.")
endif()
