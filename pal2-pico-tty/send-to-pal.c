#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "send-to-pal.h"
#include "proj_hw.h"
#include "kim-reply-parser.h"

#define ESC "\x1b[" /* or "\033["                    */
#define RED ESC "31m"
#define YELLOW ESC "33m"
#define RESET ESC "0m"

void send_char_to_pal(char ch)
{
    uart_putc_raw(PAL_UART, (uint8_t)ch);

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
    int ch_pal = uart_getc(PAL_UART);

    printf(RED "%c" RESET, (char)ch_pal);
}

void send_line_to_pal(const char *line)
{
    size_t n = strlen(line);
    bool cr_found;

    for (size_t i = 0; i <= n; i++)
    {
        char ch = line[i];
        send_char_to_pal(ch);
        if (ch == '\r')
        {
            cr_found = true;
        }
    }

    if (!cr_found)
    {
        send_char_to_pal('\r');
        send_char_to_pal('\n');
    }

    while (uart_is_readable(PAL_UART))
    {
        int ch_pal = uart_getc(PAL_UART);
        char ch = (char)ch_pal;
        printf(YELLOW "%c" RESET, ch);
        kim_reply_parser_feed(&kim_reply_parser, ch);
    }
}
