
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
// #include "malloc.h"
#include "pico/time.h"
#include "buttons.h"

#include "sd-card/sd-card.h"

#include "font.h"

#include "blink.pio.h"
#include "ssd1306.h"
#include "proj_hw.h"
#include "tty_switch_passthrough.h"

#define USB_TIMEOUT_US (1 * 1000000)

static void reset_pal(void);
void main_loop(ssd1306_tty_t *tty);

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq)
{
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

static void wait_for_usb_with_timeout()
{
    uint64_t start_time = time_us_64();
    while (!stdio_usb_connected())
    {
        if (time_us_64() - start_time >= USB_TIMEOUT_US)
        {
            break;
        }
        tight_loop_contents();
    }
}

int main()
{
    stdio_init_all();

    wait_for_usb_with_timeout();

    configure_hardware();

    init_buttons();

    // // Initialise the Wi-Fi chip
    // if (cyw43_arch_init()) {
    //     printf("Wi-Fi init failed\n");
    //     return -1;
    // }

    /* --- UART setup ------------------------------------------------------ */
    uart_init(PAL_UART, BAUD_RATE);
    gpio_set_function(PAL_UART_TX_GPIO, GPIO_FUNC_UART);
    gpio_set_function(PAL_UART_RX_GPIO, GPIO_FUNC_UART);

    // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    // For more examples of SPI use see https://github.com/raspberrypi/pico-examples/tree/master/spi

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // PIO Blinking example
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);

#ifdef PICO_DEFAULT_LED_PIN
    blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
#else
    blink_pin_forever(pio, 0, offset, 6, 3);
#endif
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio

    // Example to turn on the Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(PAL_UART_TX_GPIO, GPIO_FUNC_UART);
    gpio_set_function(PAL_UART_RX_GPIO, GPIO_FUNC_UART);

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART

    // Send out a string, with CR/LF conversions
    uart_puts(PAL_UART, " Hello, UART!\n");

    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    printf("Scanning I²C\r\n");
    int i2c_addr = scan_i2c_bus();
    if (i2c_addr >= 0)
    {
        printf("First I²C device @ 0x%02X\r\n", i2c_addr);
    }
    else
    {
        printf("No I²C devices detected\r\n");
    }

    ssd1306_t disp;
    ssd1306_tty_t tty;

    init_ssd1306(i2c_addr, &disp);
    ssd1306_init_tty(&disp, &tty, font_8x5);

    ssd1306_tty_puts(&tty, "SCANNING DRIVE --", 0);
    ssd1306_tty_show(&tty);
    printf("Scanning drive\r\n");
    int k = prep_sd_card();
    printf("Done scanning drive\r\n");
    ssd1306_tty_puts(&tty, "Done\n", 0);
    ssd1306_tty_show(&tty);

    ssd1306_tty_puts(&tty, "Scanning RAM", 0);
    ssd1306_tty_show(&tty);
    size_t mem_size1 = get_largest_alloc_block_binary2(1, 1024 * 1024);
    size_t freeK = (size_t)(mem_size1 / 1024);

    printf("(binary) Largest chunk of free heap = %d %d\r\n", mem_size1, freeK);

    int y = 0;
    int ss = 1;

    ssd1306_tty_printf(&tty, "FREE RAM: %dK %d\n", freeK, mem_size1);
    ssd1306_tty_show(&tty);

    switch_passthrough_init();

    main_loop(&tty);
    while (false)
    {
        //  printf("Hello, world!\n");
        sleep_ms(1000);
    }
}

static void reset_pal(void)
{
    /* Assert reset (active‑low) for 100 ms */
    gpio_set_dir(PAL_RESET_GPIO, GPIO_OUT);
    gpio_put(PAL_RESET_GPIO, 0);
    sleep_ms(100);
    gpio_set_dir(PAL_RESET_GPIO, GPIO_IN); /* release */
}

void main_loop(ssd1306_tty_t *tty)
{
    ssd1306_tty_puts(tty, "Restting PAL...", 0);
    ssd1306_tty_show(tty);

    reset_pal();
    ssd1306_tty_puts(tty, " done\n", 0);
    ssd1306_tty_show(tty);

    ssd1306_tty_puts(tty, "Boot successful, in main loop\n", 0);
    ssd1306_tty_show(tty);

    while (true)
    {
        // if (btn.rewind)
        // {
        //     ssd1306_tty_puts(tty, "REWIND pressed\n", 0);
        // }
        // if (btn.play)
        // {
        //     ssd1306_tty_puts(tty, "PLAY pressed\n", 0);
        // }
        // if (btn.fast_forward)
        // {
        //     ssd1306_tty_puts(tty, "FAST FORWARD pressed\n", 0);
        // }
        // if (btn.record)
        // {
        //     ssd1306_tty_puts(tty, "RECORD pressed\n", 0);
        // }
        // ssd1306_tty_show(tty);

        bool idle = true;

        /* USB‑>PAL */
        int ch_usb = getchar_timeout_us(0);
        if (ch_usb != PICO_ERROR_TIMEOUT)
        {
            uart_putc_raw(PAL_UART, (uint8_t)ch_usb);
            idle = false;
        }

        /* PAL‑>USB */
        if (uart_is_readable(PAL_UART))
        {
            int ch_pal = uart_getc(PAL_UART);
            putchar_raw(ch_pal);
            idle = false;
        }

        if (idle)
        {
            button_state_t btn = read_buttons_struct();

            if (btn.menu == BUTTON_STATE_PRESSED)
            {
                ssd1306_tty_puts(tty, "MENU pressed\n", 0);

                process_menu(tty);

                ssd1306_tty_cls(tty);
                ssd1306_tty_puts(tty, " LEFT MENU\n", 0);
                ssd1306_tty_show(tty);
            }
            else
            {
                sleep_ms(10);
            }
        }
    }
}