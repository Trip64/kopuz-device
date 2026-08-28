cmake_minimum_required(VERSION 3.20)

# Project name
project(Helios C ASM)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# MCU specific flags
set(MCU_FLAGS "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")

# Compile definitions
add_compile_definitions(
    STM32F746xx
    USE_HAL_DRIVER
)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MCU_FLAGS} -Wall -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS_DEBUG "-g -gdwarf-2 -O0")
set(CMAKE_C_FLAGS_RELEASE "-Os")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MCU_FLAGS} -specs=nano.specs -T${CMAKE_SOURCE_DIR}/STM32F746ZGTx_FLASH.ld -Wl,--gc-sections -lc -lm")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F7xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F7xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Middlewares/FatFs
)

# Source files
file(GLOB_RECURSE SOURCES
    "Core/Src/*.c"
    "Drivers/STM32F7xx_HAL_Driver/Src/*.c"
    "Middlewares/FatFs/ff.c"
    "startup_stm32f746xx.s"
)

# Executable
add_executable(${PROJECT_NAME}.elf ${SOURCES})

# Generate bin and hex files
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Building ${PROJECT_NAME}.bin and ${PROJECT_NAME}.hex"
)
