
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
#include "malloc.h"
#include "bt_main.h"
#include "pico/multicore.h"
#include "btstack.h"

#include "sd-card/sd-card.h"
#include "proj_hw.h"
#include "buttons.h"
#include "blink.pio.h"
#include "ssd1306.h"
#include "proj_hw.h"
#include "pal-io.h"

#include "ring.h"
#include "config.h"
#include "debug.h"

static void ssd1306_set_status(ssd1306_t *disp, const char *s);
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

bool wait_for_usb_connection(uint timeout_ms)
{
    const absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

    while (!stdio_usb_connected())
    {
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0)
        {
            return false; // Timed out
        }
        sleep_ms(10); // Polling interval
    }
    return true; // USB connected
}

bool repeating_timer_callback(struct repeating_timer *t)
{
    // Alright, I had to put the request on a timer to allow bytes
    // to get batched up in the ring buffer.  Otherwise, it would try
    // to send ONE CHARACTER per batch at 9600 baud, which bogged down
    // the system.
    //

    if (system_config.bt_connected)
    {
        if (!ring_is_empty(&tx_ring))
        {
            rfcomm_request_can_send_now_event(system_config.rfcomm_channel_id);
        }
    }

    return true; // Keep repeating
}

void core1_main(void)
{
    struct repeating_timer timer;
    add_repeating_timer_ms(100, repeating_timer_callback, NULL, &timer);

    pal_io_init();

    // jjz -> configure_hardware();

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
    gpio_pull_up(I2C_SDA); // Set up our UART
    uart_init(PAL_UART, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(PAL_UART_TX_GPIO, GPIO_FUNC_UART);
    gpio_set_function(PAL_UART_RX_GPIO, GPIO_FUNC_UART);

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART

    // Send out a string, with CR/LF conversions
    uart_puts(PAL_UART, " Hello, UART!\n");

    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    debug_printf("Scanning I²C\r\n");
    int i2c_addr = scan_i2c_bus();

    if (i2c_addr >= 0)
    {
        debug_printf("First I²C device @ 0x%02X\r\n", i2c_addr);
    }
    else
    {
        debug_printf("No I²C devices detected\r\n");
    }

    ssd1306_t disp;
    ssd1306_tty_t tty;

    init_ssd1306(i2c_addr, &disp);
    ssd1306_init_tty(&disp, &tty, get_font());

    ssd1306_set_status(&disp, "SCANNING DRIVE");
    debug_printf("Scanning drive\r\n");
    int k = prep_sd_card();
    debug_printf("Done scanning drive\r\n");
    ssd1306_set_status(&disp, "SCAN COMPLETE");

    load_config_from_sd();
    save_config_to_sd();

    /* --- UART setup ------------------------------------------------------ */
    uart_init(PAL_UART, user_config.baud);
    gpio_set_function(PAL_UART_TX_GPIO, GPIO_FUNC_UART);
    gpio_set_function(PAL_UART_RX_GPIO, GPIO_FUNC_UART);

    ssd1306_set_status(&disp, "SCANNING RAM");
    size_t mem_size1 = get_largest_alloc_block_binary2(1, 1024 * 1024);
    size_t freeK = (size_t)(mem_size1 / 1024);

    debug_printf("(binary) Largest chunk of free heap = %d %d\r\n", mem_size1, freeK);

    int y = 0;
    int ss = 1;
    ssd1306_clear(&disp);
    ssd1306_printf(&disp, 0, y, ss, "FREE RAM:");
    ssd1306_printf(&disp, 0, y + (8 * ss * 1), ss, "%dK", freeK);
    ssd1306_printf(&disp, 0, y + (8 * ss * 2), ss, "%d\r\n", mem_size1);
    ssd1306_show(&disp);

    init_buttons();

    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c


    main_loop(&tty);
}

int main()
{
    stdio_init_all();

    ring_init(&tx_ring);
    ring_init(&rx_ring);

    bool connected = wait_for_usb_connection(1000);

    multicore_launch_core1(core1_main); // 1️⃣ start core-1 and its IO
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init())
    {
        printf("cyw43_arch_init() failed.\n");
        return -1;
    }

    // Example to turn on the Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    bt_main();
    while (false)
    {
        tight_loop_contents();
    }
}

static void ssd1306_set_status(ssd1306_t *disp, const char *s)
{
    ssd1306_clear(disp);
    ssd1306_draw_string(disp, 0, 24, 1, s);
    ssd1306_show(disp);
}

void show_default_text(ssd1306_tty_t *tty)
{
    ssd1306_tty_cls(tty);
    ssd1306_tty_puts(tty, " TTY MODE\n");
    ssd1306_tty_puts(tty, " USB<->PAL2\n");
    if (system_config.tty_mode)
    {
        ssd1306_tty_puts(tty, " TTY MODE\n");
    }
    else
    {
        ssd1306_tty_puts(tty, " PAL2 MODE\n");
    }

    ssd1306_tty_printf(tty, "Type '%c' to toggle.\n", user_config.toggle_char);

    if (system_config.file_status == FILE_STATUS_GOOD)
    {
        ssd1306_tty_puts(tty, "\n UPLOAD SUCCESSFUL\n");
    }
    if (system_config.file_status == FILE_STATUS_ERROR)
    {
        ssd1306_tty_puts(tty, "\n UPLOAD ERROR\n");
    }

    ssd1306_tty_show(tty);
}

void main_loop(ssd1306_tty_t *tty)
{
    reset_pal(tty);
    show_default_text(tty);
    u_reset_terminal();

    while (true)
    {
        bool idle = true;

        /* USB‑>PAL */
        int ch_user = user_getchar();

        if (ch_user > -1)
        {
            if ((char)ch_user == user_config.toggle_char)
            {
                u_reset_terminal();

                if (system_config.tty_mode)
                {
                    u_printf("TTY MODE DISABLED.\n");
                    disable_tty_mode(tty);
                }
                else
                {
                    u_printf("TTY MODE ENABLED.\n");
                    enable_tty_mode(tty);
                }
                show_default_text(tty);
                u_reset_terminal();
                continue;
            }

            pal_putc((uint8_t)ch_user);
            idle = false;
        }

        /* PAL‑>USB */
        int ch_pal = pal_getchar();
        if (ch_pal > -1)
        {
            u_putc(ch_pal);
            idle = false;
        }

        if (idle)
        {
            button_state_t btn = read_buttons_struct(tty);

            if (btn.menu == BUTTON_STATE_PRESSED)
            {
                ssd1306_tty_puts(tty, "MENU pressed\n");

                process_menu(tty);
                show_default_text(tty);

                // Just in case something got left in a weird state
                u_reset_terminal();
            }
            else
            {
                sleep_ms(10);
            }
        }
    }
}
