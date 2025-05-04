#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "ssd1306.h"
    // #include "pico/stdlib.h"
    // #include "hardware/spi.h"
    // #include "hardware/i2c.h"
    // #include "hardware/pio.h"
    // #include "pico/cyw43_arch.h"
    // #include "hardware/uart.h"
    // #include "pico/binary_info.h"
    // #include "malloc.h"

    // SPI Defines
    // We are going to use SPI 0, and allocate it to the following GPIO pins
    // Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments

#define SPI_PORT spi1
#define PIN_MISO SD_MISO
#define PIN_CS SD_CS
#define PIN_SCK SD_SCK
#define PIN_MOSI SD_MOSI


// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 20
#define I2C_SCL 21

    static int BAUD_RATE = 9600;

#define PAL_RESET_GPIO 16
#define TTY_SWITCH2_OUTPUT 14
#define TTY_SWITCH1_INPUT 15 /* reserved – not used in this port           */

#define PAL_UART uart0
#define PAL_UART_TX_GPIO 0
#define PAL_UART_RX_GPIO 1

    bool __no_inline_not_in_flash_func(get_bootsel_button)();
    void test_button(void);

    int scan_i2c_bus(void);
    void init_ssd1306(int addr, ssd1306_t *p);
    size_t get_largest_alloc_block_binary2(size_t low, size_t high);

    bool configure_hardware(void);

    void _error_blink(int count);
    void _toggle_led();
    void _toggle_led();
    void _set_led(bool flag);

#ifdef __cplusplus
}
#endif
