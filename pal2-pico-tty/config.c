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
    .hard_reset = true,
    .toggle_char = '~'};

#define CONFIG_FILENAME "config.txt"
#define MAX_LINE_LEN 128

static bool parse_bool(const char *s)
{
    return strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0;
}

static void trim(char *s)
{
    char *end;
    while (isspace((unsigned char)*s))
        s++;
    if (*s == 0)
        return;

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
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

        // if (strcasecmp(key, "usb_connected") == 0)
        //     system_config.usb_connected = parse_bool(value);
        // else if (strcasecmp(key, "oled_address") == 0)
        //     system_config.oled_address = (uint8_t)atoi(value);
        // else if (strcasecmp(key, "tty_mode") == 0)
        //     system_config.tty_mode = parse_bool(value);
        // else if (strcasecmp(key, "soft_version") == 0)
        //     system_config.soft_version = strdup(value);  // Note: watch for heap use

        if (strcasecmp(key, "baud") == 0)
            user_config.baud = (uint32_t)atoi(value);
        else if (strcasecmp(key, "hard_reset") == 0)
            user_config.hard_reset = parse_bool(value);
        else if (strcasecmp(key, "toggle_char") == 0)
            user_config.toggle_char = value[0];
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
             "baud = %u\n"
             "hard_reset = %s\n"
             "toggle_char = %c\n",
             user_config.baud,
             user_config.hard_reset ? "true" : "false",
             user_config.toggle_char);

    UINT bw;
    FRESULT fr = f_write(&file, buf, strlen(buf), &bw);

    f_close(&file);
    
    return (fr == FR_OK && bw == strlen(buf));
}
