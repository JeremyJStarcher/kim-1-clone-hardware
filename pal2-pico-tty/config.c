#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "./sd-card/pico_fatfs/fatfs/ff.h"

#define BAUD_KEY_NAME "baud"
#define CH_DELAY_KEY_NAME "ch_delay"
#define LINE_DELAY_KEY_NAME "line_delay"
#define USE_HARD_RESET_KEY_NAME "use_hard_reset"
#define TOGGLE_CHAR_KEY_NAME "toggle_char"
#define FORCE_UPPER_CASE_KEY_NAME "force_upper_case"
#define BS_TO_DEL_KEY_NAME "bs_to_del"

#define CONFIG_FILENAME "config.txt"
#define MAX_LINE_LEN 128
#define MAX_CONFIG_SIZE 1024

#include "config.h"

ring_t tx_ring;
ring_t rx_ring;

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

#define CONFIG_FILENAME "config.txt"
#define MAX_LINE_LEN 128

static char *bool_to_str(bool b)
{
    return b ? "true" : "false";
}
static char *uint16_to_str(uint16_t n)
{
    static char buf[6]; // 5 digits + NUL
    snprintf(buf, sizeof(buf), "%u", n);
    return buf;
}

static bool parse_bool(const char *s)
{
    return strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0;
}

static uint16_t parse_uint16(const char *s)
{
    return (uint16_t)atoi(s);
}

/* Destructively strip leading and trailing ASCII whitespace. */
static void trim(char *str)
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

char *get_config_by_key(const char *key1, const char *bigbuf1)
{
    char buff2[MAX_CONFIG_SIZE];
    strncpy(buff2, bigbuf1, sizeof(buff2));
    printf("get_config_by_key: %s\n", key1);

    char *saveptr;
    char *line = strtok_r((char *)buff2, "\n", &saveptr);
    while (line)
    {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = 0;
        char *key = line;
        char *value = eq + 1;
        trim(key);
        trim(value);

        if (strcasecmp(key, key1) == 0)
        {
            printf("Found key: %s, value: %s\n", key, value);
            return value;
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    return NULL;
}

// Load config from a text file
bool load_config_from_sd(void)
{
    FIL file;
    if (f_open(&file, CONFIG_FILENAME, FA_READ) != FR_OK)
    {
        return false;
    }

    char big_buf[MAX_CONFIG_SIZE];
    f_read(&file, big_buf, sizeof(big_buf), NULL);
    f_close(&file);

    if (get_config_by_key(BAUD_KEY_NAME, big_buf))
    {
        user_config.baud = parse_uint16(get_config_by_key(BAUD_KEY_NAME, big_buf));
    }

    if (get_config_by_key(CH_DELAY_KEY_NAME, big_buf))
    {
        user_config.ch_delay = parse_uint16(get_config_by_key(CH_DELAY_KEY_NAME, big_buf));
    }
    if (get_config_by_key(LINE_DELAY_KEY_NAME, big_buf))
    {
        user_config.line_delay = parse_uint16(get_config_by_key(LINE_DELAY_KEY_NAME, big_buf));
    }
    if (get_config_by_key(USE_HARD_RESET_KEY_NAME, big_buf))
    {
        user_config.use_hard_reset = parse_bool(get_config_by_key(USE_HARD_RESET_KEY_NAME, big_buf));
    }
    if (get_config_by_key(TOGGLE_CHAR_KEY_NAME, big_buf))
    {
        user_config.toggle_char = get_config_by_key(TOGGLE_CHAR_KEY_NAME, big_buf)[0];
    }
    if (get_config_by_key(FORCE_UPPER_CASE_KEY_NAME, big_buf))
    {
        user_config.force_upper_case = parse_bool(get_config_by_key(FORCE_UPPER_CASE_KEY_NAME, big_buf));
    }
    if (get_config_by_key(BS_TO_DEL_KEY_NAME, big_buf))
    {
        user_config.bs_to_del = parse_bool(get_config_by_key(BS_TO_DEL_KEY_NAME, big_buf));
    }
}

static void catit_str(char *buf, size_t buf_size, const char *key, const char *value)
{
    strncat(buf, key, buf_size - strlen(buf) - 1);
    strncat(buf, " = ", buf_size - strlen(buf) - 1);
    strncat(buf, value, buf_size - strlen(buf) - 1);
    strncat(buf, "\n", buf_size - strlen(buf) - 1);
}

static void catit_uint16(char *buf, size_t buf_size, const char *key, const uint16_t value)
{
    char value_str[6]; // 5 digits + NUL
    snprintf(value_str, sizeof(value_str), "%u", value);
    strncat(buf, key, buf_size - strlen(buf) - 1);
    strncat(buf, " = ", buf_size - strlen(buf) - 1);
    strncat(buf, value_str, buf_size - strlen(buf) - 1);
    strncat(buf, "\n", buf_size - strlen(buf) - 1);
}
static void catit_bool(char *buf, size_t buf_size, const char *key, const bool value)
{
    strncat(buf, key, buf_size - strlen(buf) - 1);
    strncat(buf, " = ", buf_size - strlen(buf) - 1);
    strncat(buf, bool_to_str(value), buf_size - strlen(buf) - 1);
    strncat(buf, "\n", buf_size - strlen(buf) - 1);
}

static void catit_char(char *buf, size_t buf_size, const char *key, const char value)
{
    char value_str[2];
    snprintf(value_str, sizeof(value_str), "%c", value);
    strncat(buf, key, buf_size - strlen(buf) - 1);
    strncat(buf, " = ", buf_size - strlen(buf) - 1);
    strncat(buf, value_str, buf_size - strlen(buf) - 1);
    strncat(buf, "\n", buf_size - strlen(buf) - 1);
}

// Save config to a text file
bool save_config_to_sd(void)
{
    FIL file;
    if (f_open(&file, CONFIG_FILENAME, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return false;

    char buf[1024];
    char buf2[1024];

    buf[0] = 0; // Initialize buffer to empty string

    catit_str(buf, sizeof(buf), "ZZ", "ZZZ");
    catit_uint16(buf, sizeof(buf), BAUD_KEY_NAME, user_config.baud);
    catit_uint16(buf, sizeof(buf), CH_DELAY_KEY_NAME, user_config.ch_delay);
    catit_uint16(buf, sizeof(buf), LINE_DELAY_KEY_NAME, user_config.line_delay);
    catit_bool(buf, sizeof(buf), USE_HARD_RESET_KEY_NAME, user_config.use_hard_reset);
    catit_char(buf, sizeof(buf), TOGGLE_CHAR_KEY_NAME, user_config.toggle_char);
    catit_bool(buf, sizeof(buf), FORCE_UPPER_CASE_KEY_NAME, user_config.force_upper_case);
    catit_bool(buf, sizeof(buf), BS_TO_DEL_KEY_NAME, user_config.bs_to_del);

    UINT bw;
    FRESULT fr = f_write(&file, buf, strlen(buf), &bw);

    f_close(&file);

    return (fr == FR_OK && bw == strlen(buf));
}
