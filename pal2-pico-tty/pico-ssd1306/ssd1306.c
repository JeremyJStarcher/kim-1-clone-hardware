/*

MIT License

Copyright (c) 2021 David Schramm

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <pico/binary_info.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "ssd1306.h"
#include "font.h"
#include "debug.h"

static bool text_inv_mode = false;
void ssd1306_tty_show2(ssd1306_tty_t *tty);

inline static void swap(int32_t *a, int32_t *b)
{
    int32_t *t = a;
    *a = *b;
    *b = *t;
}

inline static void fancy_write(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, char *name)
{
    switch (i2c_write_blocking(i2c, addr, src, len, false))
    {
    case PICO_ERROR_GENERIC:
        printf("[%s] addr not acknowledged!\n", name);
        break;
    case PICO_ERROR_TIMEOUT:
        printf("[%s] timeout!\n", name);
        break;
    default:
        // printf("[%s] wrote successfully %lu bytes!\n", name, len);
        break;
    }
}

inline static void ssd1306_write(ssd1306_t *p, uint8_t val)
{
    uint8_t d[2] = {0x00, val};
    fancy_write(p->i2c_i, p->address, d, 2, "ssd1306_write");
}

bool ssd1306_init(ssd1306_t *p, uint16_t width, uint16_t height, uint8_t address, i2c_inst_t *i2c_instance)
{
    p->width = width;
    p->height = height;
    p->pages = height / 8;
    p->address = address;

    p->i2c_i = i2c_instance;

    p->bufsize = (p->pages) * (p->width);
    if ((p->buffer = malloc(p->bufsize + 1)) == NULL)
    {
        p->bufsize = 0;
        return false;
    }

    ++(p->buffer);

    // from https://github.com/makerportal/rpi-pico-ssd1306
    uint8_t cmds[] = {
        SET_DISP,
        // timing and driving scheme
        SET_DISP_CLK_DIV,
        0x80,
        SET_MUX_RATIO,
        height - 1,
        SET_DISP_OFFSET,
        0x00,
        // resolution and layout
        SET_DISP_START_LINE,
        // charge pump
        SET_CHARGE_PUMP,
        p->external_vcc ? 0x10 : 0x14,
        SET_SEG_REMAP | 0x01,   // column addr 127 mapped to SEG0
        SET_COM_OUT_DIR | 0x08, // scan from COM[N] to COM0
        SET_COM_PIN_CFG,
        width > 2 * height ? 0x02 : 0x12,
        // display
        SET_CONTRAST,
        0xff,
        SET_PRECHARGE,
        p->external_vcc ? 0x22 : 0xF1,
        SET_VCOM_DESEL,
        0x30,          // or 0x40?
        SET_ENTIRE_ON, // output follows RAM contents
        SET_NORM_INV,  // not inverted
        SET_DISP | 0x01,
        // address setting
        SET_MEM_ADDR,
        0x00, // horizontal
    };

    for (size_t i = 0; i < sizeof(cmds); ++i)
        ssd1306_write(p, cmds[i]);

    return true;
}

inline void ssd1306_deinit(ssd1306_t *p)
{
    free(p->buffer - 1);
}

inline void ssd1306_poweroff(ssd1306_t *p)
{
    ssd1306_write(p, SET_DISP | 0x00);
}

inline void ssd1306_poweron(ssd1306_t *p)
{
    ssd1306_write(p, SET_DISP | 0x01);
}

inline void ssd1306_contrast(ssd1306_t *p, uint8_t val)
{
    ssd1306_write(p, SET_CONTRAST);
    ssd1306_write(p, val);
}

inline void ssd1306_invert(ssd1306_t *p, uint8_t inv)
{
    ssd1306_write(p, SET_NORM_INV | (inv & 1));
}

inline void ssd1306_clear(ssd1306_t *p)
{
    memset(p->buffer, 0, p->bufsize);
}

void ssd1306_clear_pixel(ssd1306_t *p, uint32_t x, uint32_t y)
{
    if (x >= p->width || y >= p->height)
        return;

    p->buffer[x + p->width * (y >> 3)] &= ~(0x1 << (y & 0x07));
}

void ssd1306_draw_pixel(ssd1306_t *p, uint32_t x, uint32_t y)
{
    if (x >= p->width || y >= p->height)
        return;

    p->buffer[x + p->width * (y >> 3)] |= 0x1 << (y & 0x07); // y>>3==y/8 && y&0x7==y%8
}

void ssd1306_draw_line(ssd1306_t *p, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    if (x1 > x2)
    {
        swap(&x1, &x2);
        swap(&y1, &y2);
    }

    if (x1 == x2)
    {
        if (y1 > y2)
            swap(&y1, &y2);
        for (int32_t i = y1; i <= y2; ++i)
            ssd1306_draw_pixel(p, x1, i);
        return;
    }

    float m = (float)(y2 - y1) / (float)(x2 - x1);

    for (int32_t i = x1; i <= x2; ++i)
    {
        float y = m * (float)(i - x1) + (float)y1;
        ssd1306_draw_pixel(p, i, (uint32_t)y);
    }
}

void ssd1306_clear_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    for (uint32_t i = 0; i < width; ++i)
        for (uint32_t j = 0; j < height; ++j)
            ssd1306_clear_pixel(p, x + i, y + j);
}

void ssd1306_draw_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    for (uint32_t i = 0; i < width; ++i)
        for (uint32_t j = 0; j < height; ++j)
            ssd1306_draw_pixel(p, x + i, y + j);
}

void ssd1306_draw_empty_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    ssd1306_draw_line(p, x, y, x + width, y);
    ssd1306_draw_line(p, x, y + height, x + width, y + height);
    ssd1306_draw_line(p, x, y, x, y + height);
    ssd1306_draw_line(p, x + width, y, x + width, y + height);
}

void ssd1306_draw_char_with_font(ssd1306_t *p, uint32_t x, uint32_t y, int scale, const uint8_t *font, char c)
{
    if (c < font[3] || c > font[4])
        return;

    uint32_t parts_per_line = (font[0] >> 3) + ((font[0] & 7) > 0);
    for (uint8_t w = 0; w < font[1]; ++w)
    { // width
        uint32_t pp = (c - font[3]) * font[1] * parts_per_line + w * parts_per_line + 5;
        for (uint32_t lp = 0; lp < parts_per_line; ++lp)
        {
            uint8_t line = font[pp];

            for (int8_t j = 0; j < 8; ++j, line >>= 1)
            {
                bool bit = !(line & 1);

                if (bit == text_inv_mode)
                {
                    ssd1306_draw_square(p, x + w * scale, y + ((lp << 3) + j) * scale, scale, scale);
                }
                else
                {
                    ssd1306_clear_square(p, x + w * scale, y + ((lp << 3) + j) * scale, scale, scale);
                }
            }

            ++pp;
        }
    }
}

void ssd1306_draw_string_with_font(ssd1306_t *p, uint32_t x, uint32_t y, int scale, const uint8_t *font, const char *s)
{

    if (text_inv_mode)
    {
        // Add a little padding so we get a top and bottom border
        ssd1306_draw_square(p, x, y - 1, strlen(s) * ((font[1] + font[2]) * scale), (font[0] * scale) + 2);
    }

    for (int32_t x_n = x; *s; x_n += (font[1] + font[2]) * scale)
    {
        ssd1306_draw_char_with_font(p, x_n, y, scale, font, *(s++));
    }
}

void ssd1306_draw_char(ssd1306_t *p, uint32_t x, uint32_t y, int scale, char c)
{
    ssd1306_draw_char_with_font(p, x, y, scale, font_8x5, c);
}

void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, int scale, const char *s)
{
    ssd1306_draw_string_with_font(p, x, y, scale, font_8x5, s);
}

static inline uint32_t ssd1306_bmp_get_val(const uint8_t *data, const size_t offset, uint8_t size)
{
    switch (size)
    {
    case 1:
        return data[offset];
    case 2:
        return data[offset] | (data[offset + 1] << 8);
    case 4:
        return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
    default:
        __builtin_unreachable();
    }
    __builtin_unreachable();
}

void ssd1306_bmp_show_image_with_offset(ssd1306_t *p, const uint8_t *data, const long size, uint32_t x_offset, uint32_t y_offset)
{
    if (size < 54) // data smaller than header
        return;

    const uint32_t bfOffBits = ssd1306_bmp_get_val(data, 10, 4);
    const uint32_t biSize = ssd1306_bmp_get_val(data, 14, 4);
    const uint32_t biWidth = ssd1306_bmp_get_val(data, 18, 4);
    const int32_t biHeight = (int32_t)ssd1306_bmp_get_val(data, 22, 4);
    const uint16_t biBitCount = (uint16_t)ssd1306_bmp_get_val(data, 28, 2);
    const uint32_t biCompression = ssd1306_bmp_get_val(data, 30, 4);

    if (biBitCount != 1) // image not monochrome
        return;

    if (biCompression != 0) // image compressed
        return;

    const int table_start = 14 + biSize;
    uint8_t color_val = 0;

    for (uint8_t i = 0; i < 2; ++i)
    {
        if (!((data[table_start + i * 4] << 16) | (data[table_start + i * 4 + 1] << 8) | data[table_start + i * 4 + 2]))
        {
            color_val = i;
            break;
        }
    }

    uint32_t bytes_per_line = (biWidth / 8) + (biWidth & 7 ? 1 : 0);
    if (bytes_per_line & 3)
        bytes_per_line = (bytes_per_line ^ (bytes_per_line & 3)) + 4;

    const uint8_t *img_data = data + bfOffBits;

    int32_t step = biHeight > 0 ? -1 : 1;
    int32_t border = biHeight > 0 ? -1 : -biHeight;

    for (uint32_t y = biHeight > 0 ? biHeight - 1 : 0; y != (uint32_t)border; y += step)
    {
        for (uint32_t x = 0; x < biWidth; ++x)
        {
            if (((img_data[x >> 3] >> (7 - (x & 7))) & 1) == color_val)
                ssd1306_draw_pixel(p, x_offset + x, y_offset + y);
        }
        img_data += bytes_per_line;
    }
}

inline void ssd1306_bmp_show_image(ssd1306_t *p, const uint8_t *data, const long size)
{
    ssd1306_bmp_show_image_with_offset(p, data, size, 0, 0);
}

void ssd1306_show(ssd1306_t *p)
{
    uint8_t payload[] = {SET_COL_ADDR, 0, p->width - 1, SET_PAGE_ADDR, 0, p->pages - 1};
    if (p->width == 64)
    {
        payload[1] += 32;
        payload[2] += 32;
    }

    for (size_t i = 0; i < sizeof(payload); ++i)
        ssd1306_write(p, payload[i]);

    *(p->buffer - 1) = 0x40;

    fancy_write(p->i2c_i, p->address, p->buffer - 1, p->bufsize + 1, "ssd1306_show");
}

void ssd1306_set_text_inv(ssd1306_t *p, const bool mode)
{
    text_inv_mode = mode;
}

void ssd1306_printf(ssd1306_t *p, uint32_t x, uint32_t y, int scale, const char *fmt, ...)
{
    char buf[256]; /* adjust to a sensible upper bound */
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ssd1306_draw_string(p, x, y, scale, buf);
}

static void aaa(ssd1306_t *p);

#define MAX_TTY_X 80
#define MAX_TTY_Y 25

void ssd1306_tty_set_scale(ssd1306_tty_t *tty, int scale)
{

    tty->font_height = (tty->font[0] * scale);
    tty->font_width = (tty->font[1] * scale + tty->font[2]);
    tty->scale = scale;
    tty->height = tty->ssd1306->height / tty->font_height;
    tty->width = tty->ssd1306->width / tty->font_width;
    debug_printf("TTY CONFIG: height/width %d/%d\n", tty->height, tty->width);
}

void ssd1306_tty_set_font(ssd1306_tty_t *tty, const uint8_t *font, int scale)
{
    tty->font_height = (font[0] * scale);
    tty->font_width = (font[1] * scale + font[2]);
    tty->font = font;
    ssd1306_tty_set_scale(tty, scale);

    debug_printf("TTY CONFIG: height/width %d/%d\n", tty->height, tty->width);
}

void ssd1306_tty_scroll(ssd1306_tty_t *tty)
{
    // Move all rows up by one
    int row_size = tty->width;
    memmove(tty->buffer, tty->buffer + row_size, (tty->height - 1) * row_size);
    memmove(tty->color, tty->color + row_size, (tty->height - 1) * row_size);

    // Clear the last row
    memset(tty->buffer + (tty->height - 1) * row_size, ' ', row_size);
    memset(tty->color + (tty->height - 1) * row_size, 0, row_size);

    tty->y = tty->height - 1;
}

void ssd1306_tty_cls(ssd1306_tty_t *tty)
{
    ssd1306_clear(tty->ssd1306);
    memset(tty->buffer, ' ', tty->width * tty->height);
    memset(tty->color, 0, tty->width * tty->height);
    tty->x = 0;
    tty->y = 0;

    // Optional: call a render function here
    // ssd1306_tty_render(tty);
}

void ssd1306_tty_puts(ssd1306_tty_t *tty, const char *s)
{
    while (*s)
    {
        ssd1306_tty_writechar(tty, *s++);
    }
}

void ssd1306_tty_writechar(ssd1306_tty_t *tty, char c)
{
    if (c == '\n')
    {
        tty->x = 0;
        tty->y++;
        if (tty->y >= tty->height)
        {
            ssd1306_tty_scroll(tty);
        }
        return;
    }

    if (tty->x >= tty->width)
    {
        tty->x = 0;
        tty->y++;
        if (tty->y >= tty->height)
        {
            ssd1306_tty_scroll(tty);
        }
    }

    int idx = tty->y * tty->width + tty->x;
    tty->buffer[idx] = c;
    // tty->color[idx] = color;

    tty->x++;
}

void ssd1306_tty_dump(ssd1306_tty_t *tty)
{
    printf("DUMP %d %d\n", tty->width, tty->height);
    for (int y = 0; y < tty->height; y++)
    {
        for (int x = 0; x < tty->width; x++)
        {
            char c = tty->buffer[y * tty->width + x];
            putchar((c >= 32 && c <= 126) ? c : '.'); // printable ASCII or placeholder
        }
        putchar('\n');
    }
    printf("---> DUMP\n");
}

void ssd1306_tty_show2(ssd1306_tty_t *tty)
{
    for (int y = 0; y < tty->height; y++)
    {
        for (int x = 0; x < tty->width; x++)
        {
            char c = tty->buffer[y * tty->width + x];
            // putchar((c >= 32 && c <= 126) ? c : '.'); // printable ASCII or placeholder

            int px = x * (tty->font_width);
            int py = y * (tty->font_height);

            ssd1306_draw_char_with_font(tty->ssd1306, px, py, tty->scale, tty->font, c);
        }
    }
    ssd1306_show(tty->ssd1306);
}

void ssd1306_tty_show(ssd1306_tty_t *tty)
{
    // ssd1306_tty_show2(tty);               // original routine
    // return;

    uint64_t t0 = time_us_64();      // start‑stamp
    ssd1306_tty_show2(tty);          // original routine
    uint64_t dt = time_us_64() - t0; // elapsed

    debug_printf("Time to draw screen %" PRIu64 " µs\n", dt);
    //    return (uint32_t)dt;                 // ≤ ~71 min fits in 32 bits
}

void ssd1306_init_tty(ssd1306_t *p, ssd1306_tty_t *tty, const uint8_t *font)
{
    tty->ssd1306 = p;
    tty->bufsize = MAX_TTY_X * MAX_TTY_Y;
    tty->buffer = malloc(MAX_TTY_X * MAX_TTY_Y);
    tty->color = malloc(MAX_TTY_X * MAX_TTY_Y);

    ssd1306_tty_set_font(tty, font, 1);

    ssd1306_tty_cls(tty);

    // ssd1306_tty_puts(tty, "Line 1\n", 0);
    // ssd1306_tty_puts(tty, "Line 2\n", 0);
    // ssd1306_tty_puts(tty, "Line 3\n", 0);
    // ssd1306_tty_puts(tty, "Line 4\n", 0);
    // ssd1306_tty_puts(tty, "Line 5\n", 0);
    // ssd1306_tty_puts(tty, "Line 6\n7!\n8888888888888888888888888888", 0);
    // ssd1306_tty_dump(tty);

    ssd1306_tty_show(tty);
    // aaa(p);
}

void ssd1306_tty_printf(ssd1306_tty_t *tty, const char *fmt, ...)
{
    char buf[256]; /* adjust to a sensible upper bound */
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    ssd1306_tty_puts(tty, buf);
}

static void aaa(ssd1306_t *p)
{

#define SLEEPTIME 25

    for (int y = 0; y < 31; ++y)
    {
        ssd1306_draw_line(p, 0, y, 127, y);
        ssd1306_show(p);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(p);
    }

    for (int y = 0, i = 1; y >= 0; y += i)
    {
        ssd1306_draw_line(p, 0, 31 - y, 127, 31 + y);
        ssd1306_draw_line(p, 0, 31 + y, 127, 31 - y);
        ssd1306_show(p);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(p);
        if (y == 32)
            i = -1;
    }

    for (int y = 31; y < 63; ++y)
    {
        ssd1306_draw_line(p, 0, y, 127, y);
        ssd1306_show(p);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(p);
    }
}
