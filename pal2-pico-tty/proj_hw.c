#include "pico/stdlib.h"
#include "hardware/regs/io_qspi.h"
#include "hardware/regs/sio.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/irq.h"
#include "pico/platform.h" // for __no_inline_not_in_flash_func
#include "hardware/sync.h" // for save_and_disable_interrupts, restore_interrupts
#include "stdio.h"
#include "stdlib.h"
#include "pico/binary_info.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"

#include "proj_hw.h"

const uint32_t PIN_LED = 25; // only for Pico
static bool _picoW = true;
static bool _led = false;

static bool _check_pico_w();

bool __no_inline_not_in_flash_func(get_bootsel_button)()
{
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    uint32_t flags = save_and_disable_interrupts();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 1000; ++i)
        ;

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
#if PICO_RP2040
#define CS_BIT (1u << 1)
#else
#define CS_BIT SIO_GPIO_HI_IN_QSPI_CSN_BITS
#endif
    bool button_state = !(sio_hw->gpio_hi_in & CS_BIT);

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return button_state;
}

void test_button(void)
{

    while (true)
    {
        if (get_bootsel_button())
        {
            printf("TRUE\r\n");
        }
        else
        {
            printf("FALSE\r\n");
        }
        sleep_ms(100);
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

void init_ssd1306(int addr, ssd1306_t *disp)
{

#define SLEEPTIME 25

    disp->external_vcc = false;
    ssd1306_init(disp, 128, 64, addr, I2C_PORT);
    ssd1306_clear(disp);
    ssd1306_show(disp);

#if 0
    const char *words[] = {"PAL-2", "SSD1306", "DISPLAY", "DRIVER", "*                               "};
    char buf[8];

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
#endif
}

size_t get_largest_alloc_block_binary2(size_t low, size_t high)
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

bool configure_hardware()
{

    _picoW = _check_pico_w();
    // Pico / Pico W dependencies
    if (_picoW)
    {
        if (cyw43_arch_init())
        { // this is needed for driving LED
            printf("cyw43 init failed\r\n");
            return false;
        }
        printf("Pico W\r\n");
    }
    else
    {
        printf("Pico\r\n");
        // LED
        gpio_init(PIN_LED);
        gpio_set_dir(PIN_LED, GPIO_OUT);
    }
    _set_led(false);
    return true;
}

static bool _check_pico_w()
{
    adc_init();
    adc_gpio_init(29);   // Initialize GPIO29 for ADC
    adc_select_input(3); // ADC3 is GPIO29
    uint16_t adc_value = adc_read();
    sleep_ms(100);
    adc_value = adc_read();

    gpio_init(25);
    gpio_set_dir(25, GPIO_IN);
    bool gpio25_value = gpio_get(25);

    // Logic:
    // - ADC < ~200 (low voltage) suggests Pico W
    // - GPIO25 HIGH also suggests Pico W
    if (gpio25_value || adc_value < 200)
    {
        return true; // Probably Pico W
    }

    return false; // Probably Pico
}

void _set_led(bool flag)
{
    if (_picoW)
    {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, flag);
    }
    else
    {
        gpio_put(PIN_LED, flag);
    }
    _led = flag;
}

void _toggle_led()
{
    _set_led(!_led);
}

void _error_blink(int count)
{
    while (true)
    {
        for (int i = 0; i < count; i++)
        {
            _set_led(true);
            sleep_ms(250);
            _set_led(false);
            sleep_ms(250);
        }
        _set_led(false);
        sleep_ms(500);
    }
}
