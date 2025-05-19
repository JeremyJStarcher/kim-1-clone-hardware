// config.c
// Table-driven config loader/saver for Pico FATFS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "./sd-card/pico_fatfs/fatfs/ff.h"
#include "config.h"

#define BAUD_KEY_NAME "baud"
#define CH_DELAY_KEY_NAME "ch_delay"
#define LINE_DELAY_KEY_NAME "line_delay"
#define USE_HARD_RESET_KEY_NAME "use_hard_reset"
#define TOGGLE_CHAR_KEY_NAME "toggle_char"
#define FORCE_UPPER_CASE_KEY_NAME "force_upper_case"
#define BS_TO_DEL_KEY_NAME "bs_to_del"

#define CONFIG_FILENAME "config.ini"

#ifndef MAX_CONFIG_SIZE
#define MAX_CONFIG_SIZE 1024
#endif

#ifndef MAX_LINE_LEN
#define MAX_LINE_LEN 128
#endif

volatile ring_t tx_ring;
volatile ring_t rx_ring;

pal_config_t system_config = {
    .usb_connected = false,
    .oled_address = 0x3C,
    .tty_mode = false,
    .file_status = FILE_STATUS_NONE,
    .soft_version = "1.0.0",
    .rfcomm_channel_id = -1,
    .bt_connected = false};

user_config_t user_config = {
    .baud = 9600,
    .ch_delay = 20,
    .line_delay = 200,
    .use_hard_reset = true,
    .toggle_char = '~',
    .force_upper_case = false,
    .bs_to_del = false};

// Types of config entries
typedef enum
{
    CT_UINT16,
    CT_BOOL,
    CT_CHAR
} ConfigType;

// Mapping from key name to storage location
typedef struct
{
    const char *key;
    ConfigType type;
    void *dest;
    const char *comment;
} ConfigEntry;

static ConfigEntry cfg_map[] = {
    {BAUD_KEY_NAME, CT_UINT16, &user_config.baud, "300, 1200, 2400, 9600"},
    {CH_DELAY_KEY_NAME, CT_UINT16, &user_config.ch_delay, "Delay between characters in ms"},
    {LINE_DELAY_KEY_NAME, CT_UINT16, &user_config.line_delay, "Delay between lines, in ms"},
    {USE_HARD_RESET_KEY_NAME, CT_BOOL, &user_config.use_hard_reset, "Use the reset line"},
    {TOGGLE_CHAR_KEY_NAME, CT_CHAR, &user_config.toggle_char, "What character to use to toggle TTY mode"},
    {FORCE_UPPER_CASE_KEY_NAME, CT_BOOL, &user_config.force_upper_case, "Force all characters to upper case"},
    {BS_TO_DEL_KEY_NAME, CT_BOOL, &user_config.bs_to_del, "Send DEL (0x7F) instead of backspace (recommended)"},
};

static const size_t cfg_map_len = sizeof(cfg_map) / sizeof(cfg_map[0]);

// Trim leading/trailing ASCII whitespace in place
static void trim(char *str)
{
    char *start = str;
    while (*start && isspace((unsigned char)*start))
    {
        start++;
    }
    if (*start == '\0')
    {
        *str = '\0';
        return;
    }
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
    {
        end--;
    }
    *(end + 1) = '\0';
    if (start != str)
    {
        memmove(str, start, end - start + 2);
    }
}

// Parse a uint16_t from string
static uint16_t parse_uint16(const char *s)
{
    return (uint16_t)atoi(s);
}

// Parse a boolean from string ("true" or "1" → true)
static bool parse_bool(const char *s)
{
    return (strcasecmp(s, "true") == 0) || (strcmp(s, "1") == 0);
}

static void append_comment(char *buf, size_t cap, const char *comment)
{
    size_t used = strlen(buf);
    size_t rem = cap > used ? cap - used - 1 : 0;
    if (rem == 0)
        return;

    int n = snprintf(buf + used, rem + 1, "# %s\r\n", comment);
    if (n < 0 || (size_t)n > rem)
    {
        return;
    }
}

// Append a key = formatted_value\n into buffer safely
static void append_kv(
    char *buf,
    size_t cap,
    const char *key,
    const char *fmt,
    ...)
{
    size_t used = strlen(buf);
    size_t rem = cap > used ? cap - used - 1 : 0;
    if (rem == 0)
        return;

    va_list ap;
    va_start(ap, fmt);

    int n = snprintf(buf + used, rem + 1, "%s = ", key);
    if (n < 0 || (size_t)n > rem)
    {
        va_end(ap);
        return;
    }
    used += (size_t)n;
    rem = cap > used ? cap - used - 1 : 0;
    if (rem == 0)
    {
        va_end(ap);
        return;
    }
    n = vsnprintf(buf + used, rem + 1, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n > rem)
        return;
    used += (size_t)n;
    if (used + 2 < cap)
    {
        buf[used++] = '\r';
        buf[used++] = '\n';
        buf[used] = '\0';
    }
}

// Load config from SD-card file into user_config
bool load_config_from_sd(void)
{
    FIL file;
    if (f_open(&file, CONFIG_FILENAME, FA_READ) != FR_OK)
    {
        return false;
    }

    char line[MAX_LINE_LEN];
    while (f_gets(line, sizeof(line), &file))
    {
        trim(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;

        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        for (size_t i = 0; i < cfg_map_len; i++)
        {
            if (strcasecmp(key, cfg_map[i].key) == 0)
            {
                switch (cfg_map[i].type)
                {
                case CT_UINT16:
                    *(uint16_t *)cfg_map[i].dest = parse_uint16(val);
                    break;
                case CT_BOOL:
                    *(bool *)cfg_map[i].dest = parse_bool(val);
                    break;
                case CT_CHAR:
                    *(char *)cfg_map[i].dest = val[0];
                    break;
                }
                break;
            }
        }
    }
    f_close(&file);
    return true;
}

// Save user_config into SD-card file
bool save_config_to_sd(void)
{
    FIL file;
    if (f_open(&file, CONFIG_FILENAME,
               FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        return false;
    }

    char out[MAX_CONFIG_SIZE] = {0};
    for (size_t i = 0; i < cfg_map_len; i++)
    {
        append_comment(out, sizeof(out), cfg_map[i].comment);

        switch (cfg_map[i].type)
        {
        case CT_UINT16:
            append_kv(out, sizeof(out), cfg_map[i].key,
                      "%u", *(uint16_t *)cfg_map[i].dest);
            break;
        case CT_BOOL:
            append_kv(out, sizeof(out), cfg_map[i].key,
                      "%s", (*(bool *)cfg_map[i].dest) ? "true" : "false");
            break;
        case CT_CHAR:
            append_kv(out, sizeof(out), cfg_map[i].key,
                      "%c", *(char *)cfg_map[i].dest);
            break;
        }
    }

    UINT bw;
    FRESULT fr = f_write(&file, out, strlen(out), &bw);
    f_close(&file);
    return (fr == FR_OK && bw == strlen(out));
}
