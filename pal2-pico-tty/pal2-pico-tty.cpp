
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
#include "malloc.h"

#include "sd-card/sd-card.h"

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

#include "blink.pio.h"
#include "ssd1306.h"

static void reset_pal(void);
static void set_tty_mode(bool enable);
static int scan_i2c_bus(void);
static void init_ssd1306(int addr, ssd1306_t *p);
static size_t get_largest_alloc_block_binary2(size_t low, size_t high);
static void ssd1306_set_status(ssd1306_t *disp, const char *s);

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq)
{
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart1
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

int main()
{
    stdio_init_all();

    /* Wait until someone opens the USB serial port.                         */
    while (!stdio_usb_connected())
    {
        tight_loop_contents();
    }

    configure_hardware();

    // // Initialise the Wi-Fi chip
    // if (cyw43_arch_init()) {
    //     printf("Wi-Fi init failed\n");
    //     return -1;
    // }

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

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART

    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, " Hello, UART!\n");

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
    init_ssd1306(i2c_addr, &disp);

    ssd1306_set_status(&disp, "SCANNING DRIVE");
    printf("Scanning drive\r\n");
    int k = prep_sd_card();
    printf("Done scanning drive\r\n");
    ssd1306_set_status(&disp, "SCAN COMPLETE");

    ssd1306_set_status(&disp, "SCANNING RAM");
    size_t mem_size1 = get_largest_alloc_block_binary2(1, 512 * 1024);
    size_t freeK = (size_t)(mem_size1 / 1024);
    printf("(binary) Largest chunk of free heap = %d %d\r\n", mem_size1, freeK);
    char buf[128];
    snprintf(buf, sizeof buf, "FREE RAM: %dK",  freeK);
    ssd1306_set_status(&disp, buf);

    while (false)
    {
        //  printf("Hello, world!\n");
        sleep_ms(1000);
    }
}

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr)
{
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int scan_i2c_bus()
{
#define VERBOSE 0
    // // Enable UART so we can print status output
    // stdio_init_all();

    //  /* Wait until someone opens the USB serial port.                         */
    //  while (!stdio_usb_connected()) {
    //      tight_loop_contents();
    //  }

    int port = -1;

    // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a Pico)
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));

#if VERBOSE
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
#endif

    for (int addr = 0; addr < (1 << 7); ++addr)
    {
        if (addr % 16 == 0)
        {
#if VERBOSE
            printf("%02x ", addr);
#endif
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);

#if VERBOSE
        printf(ret < 0 ? "." : "@");
#endif

        if (!(ret < 0))
        {
            port = addr;
        }
#if VERBOSE
        printf(addr % 16 == 15 ? "\n" : "  ");
#endif
    }
#if VERBOSE
    printf("Done.\n");
#endif
    return port;
}

static void init_ssd1306(int addr, ssd1306_t *disp)
{

#define SLEEPTIME 25
    const char *words[] = {"PAL-2", "SSD1306", "DISPLAY", "DRIVER", "*                               "};

    disp->external_vcc = false;
    ssd1306_init(disp, 128, 64, addr, I2C_PORT);
    ssd1306_clear(disp);

    char buf[8];

    // while(true)
    for (int i = 0; i < sizeof(words) / sizeof(char *); ++i)
    {
        static bool m = true;

        m = !m;
        ssd1306_set_text_inv(disp, m);
        ssd1306_draw_string(disp, 0, 24, 1.5, words[i]);
        ssd1306_show(disp);
        sleep_ms(800);
        ssd1306_clear(disp);
    }
}

static size_t get_largest_alloc_block_binary2(size_t low, size_t high)
{
    // requires target_compile_definitions(pal2-pico-tty PRIVATE PICO_MALLOC_PANIC=0)

    size_t best = 0;
    while (low <= high)
    {
        size_t mid = low + (high - low) / 2;
        void *p = malloc(mid);
        if (p)
        {
            free(p);
            best = mid;
            low = mid + 1;
        }
        else
        {
            high = mid - 1;
        }
    }
    return best;
}

static void ssd1306_set_status(ssd1306_t *disp, const char *s) {
    ssd1306_clear(disp);
    ssd1306_set_text_inv(disp, false);
    ssd1306_draw_string(disp, 0, 24, 1, s);
    ssd1306_show(disp);
}