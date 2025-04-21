# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# set(PICO_BOARD pico_w CACHE STRING "Board type")

# define "w" board to use cyw43_arch_xxx() function
if(PICO_BOARD)
    unset(PICO_BOARD)
endif()
if(PICO_PLATFORM STREQUAL rp2350)
    set(PICO_BOARD pico2_w)
else()
    set(PICO_BOARD pico_w)
endif()


# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pal2-pico-tty C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pal2-pico-tty
 pal2-pico-tty.cpp
 sd-card/sd-card.c
 sd-card/pico_fatfs/fatfs/ff.c
 sd-card/pico_fatfs/fatfs/ffsystem.c
 sd-card/pico_fatfs/tf_card.c
 sd-card/pico_fatfs/fatfs/ffunicode.c
)

pico_set_program_name(pal2-pico-tty "pal2-pico-tty")
pico_set_program_version(pal2-pico-tty "0.1")

# Generate PIO header
pico_generate_pio_header(pal2-pico-tty ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pal2-pico-tty 0)
pico_enable_stdio_usb(pal2-pico-tty 1)

# Add the standard library to the build
target_link_libraries(pal2-pico-tty
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(pal2-pico-tty PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(pal2-pico-tty
        hardware_spi
        hardware_i2c
        hardware_pio
        pico_cyw43_arch_none
        hardware_adc
        )

pico_add_extra_outputs(pal2-pico-tty)

