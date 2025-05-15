#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "./sd-card/pico_fatfs/fatfs/ff.h"

#define CONFIG_FILENAME "config.txt"
#define MAX_LINE_LEN 128

#include "config.h"

pal_config_t system_config = {
    .usb_connected = false,
    .oled_address = 0x3C,
    .tty_mode = true,
    .soft_version = "1.0.0"};

user_config_t user_config = {
    .baud = 9600,
    .ch_delay = 20,
    .line_delay = 200,
    .use_hard_reset = true,
    .toggle_char = '~'};

#define CONFIG_FILENAME "config.txt"
#define MAX_LINE_LEN 128

static bool parse_bool(const char *s)
{
    return strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0;
}

#include <ctype.h>
#include <string.h>

/* Destructively strip leading and trailing ASCII whitespace. */
void trim(char *str)
{
    char *start = str; /* first non-space */
    char *end;

    /* 1. Skip leading whitespace */
    while (*start && isspace((unsigned char)*start))
        ++start;

    /* 2. If the string is all spaces, leave a single NUL and return */
    if (*start == '\0')
    {
        *str = '\0';
        return;
    }

    /* 3. Locate last non-space character */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        --end;

    /* 4. Add new terminator just after the last non-space */
    *(end + 1) = '\0';

    /* 5. Move the trimmed text to the front if we skipped leading spaces */
    if (start != str)
        memmove(str, start, end - start + 2); /* +2 to copy the NUL */
}

// Load config from a text file
bool load_config_from_sd(void)
{
    FIL file;
    if (f_open(&file, CONFIG_FILENAME, FA_READ) != FR_OK)
        return false;

    char line[MAX_LINE_LEN];
    while (f_gets(line, sizeof(line), &file))
    {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = 0;
        char *key = line;
        char *value = eq + 1;
        trim(key);
        trim(value);

        // printf("KEY/VAL %s: %s\n", key, value);

        if (strcasecmp(key, "baud") == 0)
            user_config.baud = (uint16_t)atoi(value);
        else if (strcasecmp(key, "ch_delay") == 0)
            user_config.ch_delay = (uint16_t)atoi(value);
        else if (strcasecmp(key, "line_delay") == 0)
            user_config.line_delay = (uint16_t)atoi(value);

        else if (strcasecmp(key, "use_hard_reset") == 0)
            user_config.use_hard_reset = parse_bool(value);
        else if (strcasecmp(key, "toggle_char") == 0)
        {
            // printf("VALUE[0] %d %d %d\n", value[0], value[1], value[2]);
            user_config.toggle_char = value[0];
        }
    }

    f_close(&file);
    return true;
}

// Save config to a text file
bool save_config_to_sd(void)
{
    FIL file;
    if (f_open(&file, CONFIG_FILENAME, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return false;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "baud = %d\n"
             "ch_delay = %d\n"
             "line_delay = %d\n"
             "hard_reset = %s\n"
             "toggle_char = %c\n",
             user_config.baud,
             user_config.ch_delay,
             user_config.line_delay,
             user_config.use_hard_reset ? "true" : "false",
             user_config.toggle_char);

    UINT bw;
    FRESULT fr = f_write(&file, buf, strlen(buf), &bw);

    f_close(&file);

    return (fr == FR_OK && bw == strlen(buf));
}
