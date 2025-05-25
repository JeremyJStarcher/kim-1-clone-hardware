# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

# ─── Project-wide settings ───────────────────────────────────────
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

project(pal-interface C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()



# ─── Common source files & include dirs ──────────────────────────
set(COMMON_SRCS
    pal2-pico-tty.cpp
    sd-card/sd-card.c
    sd-card/pico_fatfs/fatfs/ff.c
    sd-card/pico_fatfs/fatfs/ffsystem.c
    sd-card/pico_fatfs/tf_card.c
    sd-card/pico_fatfs/fatfs/ffunicode.c
    pico-ssd1306/ssd1306.c
    proj_hw.c
    pal-io.c
    buttons.c
    config.c
    kim-reply-parser.c
    edit-settings.c
)

set(COMMON_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}
    pico-ssd1306
    sd-card
    sd-card/pico_fatfs
)

 set(COMMON_LIBS
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_pio
    pico_cyw43_arch_none
    hardware_adc
    hardware_uart
    pico_multicore          # core-launch, FIFO, locks
    hardware_sync           # spin_lock_t (optional but handy)
)

# ─── 1) Bluetooth variant ────────────────────────────────────────
add_executable(pal-interface-bluetooth
    ${COMMON_SRCS}
    bt-serial/bt_main.c
    bt-serial/spp_counter.c
)

target_include_directories(pal-interface-bluetooth PRIVATE
    ${COMMON_INCLUDES}
    bt-serial
)
target_compile_definitions(pal-interface-bluetooth PRIVATE
    PICO_MALLOC_PANIC=0
    USE_BLUETOOTH
)
target_link_libraries(pal-interface-bluetooth PRIVATE
    ${COMMON_LIBS}
    pico_btstack_classic
    pico_btstack_cyw43
)
pico_generate_pio_header(
    pal-interface-bluetooth
    ${CMAKE_CURRENT_LIST_DIR}/blink.pio
    ${CMAKE_CURRENT_LIST_DIR}/tty_switch_passthrough.pio
)
pico_enable_stdio_uart(pal-interface-bluetooth 0)
pico_enable_stdio_usb(pal-interface-bluetooth 1)
pico_add_extra_outputs(pal-interface-bluetooth)

# ─── 2) Telnet variant ───────────────────────────────────────────
add_executable(pal-interface-telnet
    ${COMMON_SRCS}
    # no bt-serial sources here
)

target_include_directories(pal-interface-telnet PRIVATE
    ${COMMON_INCLUDES}
)
target_compile_definitions(pal-interface-telnet PRIVATE
    PICO_MALLOC_PANIC=0
    USE_TELNET
)
target_link_libraries(pal-interface-telnet PRIVATE
    ${COMMON_LIBS}
    # add any telnet‐specific libs here, e.g. pico_lwip or pico_cyw43_arch_none
)
pico_generate_pio_header(
    pal-interface-telnet
    ${CMAKE_CURRENT_LIST_DIR}/blink.pio
    ${CMAKE_CURRENT_LIST_DIR}/tty_switch_passthrough.pio
)
pico_enable_stdio_uart(pal-interface-telnet 0)
pico_enable_stdio_usb(pal-interface-telnet 1)
pico_add_extra_outputs(pal-interface-telnet)
