#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "pal-io.h"
#include "tty_switch_passthrough.pio.h"
#include "stdio.h"
#include "stdlib.h"
#include "pico/mutex.h"
#include <string.h>

#include "btstack.h"
#include "config.h"
#include "proj_hw.h"
#include "kim-reply-parser.h"
#include "ansi.h"

static mutex_t print_mutex;

#define RED ANSI_RED
#define YELLOW ANSI_YELLOW
#define RESET ANSI_RESET

#define input_gpio TTY_SWITCH1_INPUT
#define output_gpio TTY_SWITCH2_OUTPUT

static const int EDGE_BREAK = 20;
static const int WINDOW_US = (100 * 1000); // 100 ms
static PIO pio;
static int sm;

static int change_input_char(int ch)
{
    if (user_config.force_upper_case)
    {
        if (ch >= 'a' && ch <= 'z')
        {
            ch -= 0x20;
        }
    }

    if (user_config.bs_to_del)
    {
        if (ch == '\b')
        {
            ch = 0x7F; // DEL
        }
    }

    return ch;
}

static void init_switch_mirror(PIO pio, uint sm)
{
    uint offset = pio_add_program(pio, &tty_switch_passthrough_program);
    pio_sm_config c = tty_switch_passthrough_program_get_default_config(offset);

    // Init GPIOs for PIO
    pio_gpio_init(pio, TTY_SWITCH1_INPUT);
    pio_gpio_init(pio, TTY_SWITCH2_OUTPUT);

    // Start with output in Hi-Z (input mode)
    pio_sm_set_consecutive_pindirs(pio, sm, output_gpio, 1, false);

    // Map relative pin indices in the PIO program
    sm_config_set_in_pins(&c, input_gpio);      // sets pin base for wait
    sm_config_set_set_pins(&c, output_gpio, 1); // sets pin base for set
    sm_config_set_out_pins(&c, output_gpio, 1); // optional, for completeness
    sm_config_set_clkdiv(&c, 1.0f);

    // Initialize and start
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static void enable_tty_mode2()
{
    printf("Measuring  pin activity\n");
    gpio_set_dir(TTY_SWITCH1_INPUT, GPIO_IN);
    gpio_set_dir(TTY_SWITCH2_OUTPUT, GPIO_IN);

    int cnt[] = {0, 0, 0, 0};
    for (int i = 0; i <= 100000; i++)
    {

        bool is_pressed1 = gpio_get(TTY_SWITCH1_INPUT);
        if (is_pressed1)
        {
            cnt[1]++;
        }
        else
        {
            cnt[0]++;
        }

        bool is_pressed2 = gpio_get(TTY_SWITCH2_OUTPUT);
        if (is_pressed2)
        {
            cnt[3]++;
        }
        else
        {
            cnt[2]++;
        }
    }
    printf("%d %d %d %d\n", cnt[0], cnt[1], cnt[2], cnt[3]);
}

static void shutdown_switch_mirror(PIO pio, uint sm)
{
    system_config.tty_mode = false;

    // Disable the state machine
    pio_sm_set_enabled(pio, sm, false);

    // Clear instruction memory (optional but clean)
    uint offset = pio_sm_get_pc(pio, sm);
    for (int i = 0; i < 32; ++i)
    {
        pio->instr_mem[i] = pio_encode_nop();
    }

    // Clear FIFOs
    pio_sm_clear_fifos(pio, sm);

    // Unclaim the state machine
    pio_sm_unclaim(pio, sm);

    gpio_deinit(TTY_SWITCH1_INPUT);
    gpio_deinit(TTY_SWITCH2_OUTPUT);
}

static void reset_pal_inner(ssd1306_tty_t *tty2)
{
    /* Assert reset (active‑low) for 100 ms */
    gpio_set_dir(PAL_RESET_GPIO, GPIO_OUT);
    gpio_put(PAL_RESET_GPIO, 0);
    sleep_ms(100);

    int edges1 = get_edges(TTY_SWITCH1_INPUT, WINDOW_US);
    ssd1306_tty_printf(tty2, "Reset Edges: %d\n", edges1);
    ssd1306_tty_show(tty2);

    // while (edges1 > EDGE_BREAK)
    // {
    //     edges1 = get_edges(TTY_SWITCH1_INPUT, WINDOW_US);

    //     ssd1306_tty_cls(tty2);
    //     ssd1306_tty_puts(tty2, "WAITING FOR RESET\n");
    //     ssd1306_tty_printf(tty2, "Reset Edges: %d\n", edges1);
    //     ssd1306_tty_show(tty2);
    // }

    sleep_ms(100);
    gpio_set_dir(PAL_RESET_GPIO, GPIO_IN); /* release */

    // while (edges1 < EDGE_BREAK)
    // {
    //     edges1 = get_edges(TTY_SWITCH1_INPUT, WINDOW_US);
    //     int edges2 = get_edges(TTY_SWITCH2_OUTPUT, WINDOW_US);

    //     ssd1306_tty_cls(tty2);
    //     ssd1306_tty_puts(tty2, "Waiting for scan\n");
    //     ssd1306_tty_printf(tty2, "Reset Edges: %d %d\n", edges1, edges2);
    //     ssd1306_tty_show(tty2);
    // }
}

void reset_pal(ssd1306_tty_t *tty)
{
    ssd1306_tty_t tty2;
    ssd1306_t *disp = tty->ssd1306;

    // bool local_mirrored = is_tty_mode();
    // if (local_mirrored)
    // {

    //     shutdown_switch_mirror(pio, (uint)sm);
    // }

    ssd1306_init_tty(tty->ssd1306, &tty2, get_font());

    ssd1306_tty_cls(&tty2);
    ssd1306_tty_puts(&tty2, "PAL RESET\n");
    ssd1306_tty_show(&tty2);

    reset_pal_inner(&tty2);

    // if (local_mirrored)
    // {
    //     init_switch_mirror(pio, (uint)sm);
    // }

    ssd1306_tty_show(tty);
}

int get_edges(int pin, int window_us)
{
    uint32_t start = time_us_32(); // µs-resolution free-running timer
    bool last = gpio_get(pin);
    uint32_t edges = 0;

    while ((time_us_32() - start) < window_us)
    {
        bool now = gpio_get(pin);
        if (now != last)
        { // any change = one edge
            edges++;
            last = now;
        }
        /* Nothing else allowed here → pure busy-wait */
    }
    return edges;
}

void disable_tty_mode(ssd1306_tty_t *tty)
{
    shutdown_switch_mirror(pio, (uint)sm);
}

void enable_tty_mode(ssd1306_tty_t *tty)
{
    ssd1306_tty_cls(tty);
    ssd1306_tty_puts(tty, "WAITING FOR KIM\nPROMPT\n");
    ssd1306_tty_show(tty);

    disable_tty_mode(tty);

    gpio_init(PAL_RESET_GPIO);
    gpio_init(TTY_SWITCH1_INPUT);
    gpio_init(TTY_SWITCH2_OUTPUT);
    gpio_init(TTY_SWITCH1_DUP);

    gpio_set_dir(TTY_SWITCH1_INPUT, GPIO_IN);
    gpio_set_dir(TTY_SWITCH1_DUP, GPIO_IN);
    gpio_set_dir(TTY_SWITCH2_OUTPUT, GPIO_IN);
    gpio_put(TTY_SWITCH2_OUTPUT, 0);

    pio = pio0;
    sm = pio_claim_unused_sm(pio, true);
    if (sm < 0)
    {
        // Should not happen if 'required' is true, but safe check
        panic("No available state machines!");
    }

    init_switch_mirror(pio, (uint)sm);
    system_config.tty_mode = true;

    uart_set_baudrate(PAL_UART, user_config.baud);
    reset_pal(tty);
    static const char rubout_str[] = "\x7F";

    /* Spam the KIM with CRs until it responds with a prompt. */
    kim_reply_parser_init(&kim_reply_parser);

    for (int i = 0; i < 12; i++)
    {
        u_printf("SENDING RUBOUT #%d\n", i);
        upload_line_to_pal(rubout_str);
        sleep_ms(100);

        if (kim_reply_parser.prompt_seen)
        {
            break;
        }
    }

    ssd1306_tty_show(tty);
}

void upload_char_to_pal(char ch)
{
    pal_putc((uint8_t)ch);

    if (ch == '\r')
    {
        sleep_ms(user_config.line_delay);
    }
    else
    {
        sleep_ms(user_config.ch_delay);
    }

    while (!uart_is_readable(PAL_UART))
    {
        sleep_ms(1);
    }

    int ch_pal = pal_getchar();
    if (ch_pal > -1)
    {
        u_printf(RED "%c" RESET, (char)ch_pal);
    }
}

void upload_line_to_pal(const char *line)
{
    size_t n = strlen(line);
    bool cr_found;

    for (size_t i = 0; i <= n; i++)
    {
        char ch = line[i];
        upload_char_to_pal(ch);
        if (ch == '\r')
        {
            cr_found = true;
        }
    }

    if (!cr_found)
    {
        upload_char_to_pal('\r');
        upload_char_to_pal('\n');
    }

    while (true)
    {
        int ch_pal = pal_getchar();
        if (ch_pal < 0)
        {
            break;
        }

        char ch = (char)ch_pal;
        u_printf(YELLOW "%c" RESET, ch);
        kim_reply_parser_feed(&kim_reply_parser, ch);
    }
}

int user_getchar()
{
    int char_out = -1;
    int ch = getchar_timeout_us(0);
    if (ch > -1)
    {
        char_out = ch;
    }

    if (ring_count(&rx_ring) > 0)
    {
        char ch;
        ring_pop(&rx_ring, &ch);
        char_out = ch;
    }

    char_out = change_input_char(char_out);
    return char_out;
}

int pal_getchar()
{
    if (uart_is_readable(PAL_UART))
    {
        int ch_pal = uart_getc(PAL_UART);
        return ch_pal;
    }
    return -1;
}

void pal_putc(char ch)
{
    uart_putc_raw(PAL_UART, (uint8_t)ch);
}

void u_putc(char ch_pal)
{
    static bool last_was_cr = false;
    if (ch_pal == '\n')
    {
        if (!last_was_cr)
        {
            u_putc('\r'); // Insert \r before \n if not already part of \r\n
        }
    }
    if (system_config.rfcomm_channel_id > -1)
    {
        ring_push(&tx_ring, (char)ch_pal);
        // rfcomm_request_can_send_now_event handled by the interrupt
    }

    last_was_cr = (ch_pal == '\r');
    putchar_raw(ch_pal);
}

void u_puts(const char *s)
{
    while (*s)
    {
        u_putc(*s++);
    }
}

int u_printf(const char *fmt, ...)
{
    //    mutex_enter_blocking(&print_mutex);
    static char buf[256]; /* adjust to a sensible upper bound */
    va_list ap;

    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    u_puts(buf);
    return len;
    //    mutex_exit(&print_mutex);
}

void u_reset_terminal()
{
    u_puts(RESET);
}

void pal_io_init(void)
{
    mutex_init(&print_mutex);
}

void u_banner(const char *fmt, ...)
{
    u_printf("\n\n" ANSI_BG_BLUE ANSI_WHITE);

    va_list ap;

    va_start(ap, fmt);
    int len = u_printf(fmt, ap);
    va_end(ap);

    for (int i = len; i < 79; i++)
    {
        u_putc('_');
    }

    u_printf(ANSI_RESET "\n");
}