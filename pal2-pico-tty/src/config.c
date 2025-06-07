// config.c
// Table-driven config loader/saver for Pico FATFS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico_fatfs/fatfs/ff.h"
#include "config.h"

#define BAUD_KEY_NAME "baud"
#define CH_DELAY_KEY_NAME "ch_delay"
#define LINE_DELAY_KEY_NAME "line_delay"
#define USE_HARD_RESET_KEY_NAME "use_hard_reset"
#define TOGGLE_CHAR_KEY_NAME "toggle_char"
#define FORCE_UPPER_CASE_KEY_NAME "force_upper_case"
#define BS_TO_DEL_KEY_NAME "bs_to_del"
#define OLED_BRIGHTNESS_KEY_NAME "oled_brightness"
#define MENU_SCALE_KEY_NAME "menu_scale"
#define WIFI_COUNT_KEY_NAME "wifi_count"

#define CONFIG_FILENAME "config.ini"

#ifndef MAX_CONFIG_SIZE
#define MAX_CONFIG_SIZE 1024
#endif

#ifndef MAX_LINE_LEN
#define MAX_LINE_LEN 128
#endif

volatile bt_ring_t bt_tx_ring;
volatile bt_ring_t bt_rx_ring;

pal_config_t system_config = {
    .usb_connected = false,
    .oled_address = 0x3C,
    .tty_mode = false,
    .file_status = FILE_STATUS_NONE,
    .soft_version = "1.0.0",
    .rfcomm_channel_id = -1,
    .bt_connected_state = CONNECTION_STATE_NOT_CONNECTED,
    .usb_connected_state = CONNECTION_STATE_NOT_CONNECTED,
    .wifi_connected_state = CONNECTION_STATE_NOT_CONNECTED,
#ifdef USE_TELNET
    .telnetserver = NULL
#endif
};

user_config_t user_config = {
    .baud = 9600,
    .ch_delay = 20,
    .line_delay = 200,
    .use_hard_reset = true,
    .toggle_char = '~',
    .force_upper_case = false,
    .bs_to_del = false,
    .oled_brightness = 255,
    .menu_scale = 1,
    .active_wifi_count = 0,
    .wifi_aps = {{0}}};

config_entry_t cfg_map[] = {
    {BAUD_KEY_NAME, CT_UINT16, MENU_TYPE_LIST, &user_config.baud, "300, 1200, 2400, 9600", "300\t1200\t2400\t9600"},
    {CH_DELAY_KEY_NAME, CT_UINT16, MENU_TYPE_NONE, &user_config.ch_delay, "Delay between characters in ms", ""},
    {LINE_DELAY_KEY_NAME, CT_UINT16, MENU_TYPE_NONE, &user_config.line_delay, "Delay between lines, in ms", ""},
    {USE_HARD_RESET_KEY_NAME, CT_BOOL, MENU_TYPE_LIST, &user_config.use_hard_reset, "Use the reset line", "true\tfalse"},
    {TOGGLE_CHAR_KEY_NAME, CT_CHAR, MENU_TYPE_NONE, &user_config.toggle_char, "What character to use to toggle TTY mode", ""},
    {FORCE_UPPER_CASE_KEY_NAME, CT_BOOL, MENU_TYPE_LIST, &user_config.force_upper_case, "Force all characters to upper case", "true\tfalse"},
    {BS_TO_DEL_KEY_NAME, CT_BOOL, MENU_TYPE_LIST, &user_config.bs_to_del, "Send DEL (0x7F) instead of backspace (recommended)", "true\tfalse"},
    {OLED_BRIGHTNESS_KEY_NAME, CT_UINT16, MENU_TYPE_LIST, &user_config.oled_brightness, "OLED Brightness", "32\t64\t128\t192\t255"},
    {MENU_SCALE_KEY_NAME, CT_UINT16, MENU_TYPE_LIST, &user_config.menu_scale, "Menu Scale", "1\t2"},
    {WIFI_COUNT_KEY_NAME, CT_UINT16, MENU_TYPE_NONE, &user_config.active_wifi_count, "Number of configured WiFi access points", ""},
};

size_t cfg_map_len = sizeof(cfg_map) / sizeof(cfg_map[0]);

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

// Parse WiFi connection type from string
static wifi_connection_type_t parse_wifi_type(const char *s)
{
    if (strcasecmp(s, "WPA2") == 0)
        return WIFI_CONNECTION_TYPE_WPA2;
    if (strcasecmp(s, "WPA3") == 0)
        return WIFI_CONNECTION_TYPE_WPA3;
    if (strcasecmp(s, "OPEN") == 0)
        return WIFI_CONNECTION_TYPE_OPEN;
    if (strcasecmp(s, "WEP") == 0)
        return WIFI_CONNECTION_TYPE_WEP;
    return WIFI_CONNECTION_TYPE_WPA2; // default
}

// Convert WiFi connection type to string
static const char *wifi_type_to_string(wifi_connection_type_t type)
{
    switch (type)
    {
    case WIFI_CONNECTION_TYPE_WPA2:
        return "WPA2";
    case WIFI_CONNECTION_TYPE_WPA3:
        return "WPA3";
    case WIFI_CONNECTION_TYPE_OPEN:
        return "OPEN";
    case WIFI_CONNECTION_TYPE_WEP:
        return "WEP";
    default:
        return "WPA2";
    }
}

void parse_config_line(char *line)
{
    trim(line);
    if (line[0] == '\0' || line[0] == '#')
        return;

    char *eq = strchr(line, '=');
    if (!eq)
        return;

    *eq = '\0';
    char *key = line;
    char *val = eq + 1;
    trim(key);
    trim(val);

    // Handle WiFi AP entries (wifi_ap_0_ssid, wifi_ap_0_password, etc.)
    if (strncmp(key, "wifi_ap_", 8) == 0)
    {
        int ap_index = atoi(key + 8);
        if (ap_index >= 0 && ap_index < MAX_WIFI_APS)
        {
            char *field = strchr(key + 8, '_');
            if (field)
            {
                field++; // skip the underscore
                if (strcmp(field, "ssid") == 0)
                {
                    strncpy(user_config.wifi_aps[ap_index].ssid, val, MAX_WIFI_SSID_LEN);
                    user_config.wifi_aps[ap_index].ssid[MAX_WIFI_SSID_LEN] = '\0';
                    // Update active count to include this AP index
                    if (ap_index >= user_config.active_wifi_count)
                    {
                        user_config.active_wifi_count = ap_index + 1;
                    }
                }
                else if (strcmp(field, "password") == 0)
                {
                    strncpy(user_config.wifi_aps[ap_index].password, val, MAX_WIFI_PASSWORD_LEN);
                    user_config.wifi_aps[ap_index].password[MAX_WIFI_PASSWORD_LEN] = '\0';
                }
                else if (strcmp(field, "type") == 0)
                {
                    user_config.wifi_aps[ap_index].connection_type = parse_wifi_type(val);
                }
                else if (strcmp(field, "enabled") == 0)
                {
                    user_config.wifi_aps[ap_index].enabled = parse_bool(val);
                }
            }
        }
        return;
    }

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
        parse_config_line(line);
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

    // Save WiFi access points
    append_comment(out, sizeof(out), "WiFi Access Points");

    // Check if we have any non-empty WiFi configurations
    bool has_wifi_configs = false;
    for (int i = 0; i < user_config.active_wifi_count && i < MAX_WIFI_APS; i++)
    {
        if (user_config.wifi_aps[i].ssid[0] != '\0')
        {
            has_wifi_configs = true;
            break;
        }
    }

    if (!has_wifi_configs)
    {
        // Add dummy entry as example when no WiFi APs are configured
        append_comment(out, sizeof(out), "Example WiFi configuration (remove # to enable):");
        append_kv(out, sizeof(out), "#wifi_ap_0_ssid", "%s", "YourWiFiName");
        append_kv(out, sizeof(out), "#wifi_ap_0_password", "%s", "YourWiFiPassword");
        append_kv(out, sizeof(out), "#wifi_ap_0_type", "%s", "WPA2");
        append_kv(out, sizeof(out), "#wifi_ap_0_enabled", "%s", "true");
    }
    else
    {
        for (int i = 0; i < user_config.active_wifi_count && i < MAX_WIFI_APS; i++)
        {
            if (user_config.wifi_aps[i].ssid[0] != '\0')
            {
                char key_buf[64];
                snprintf(key_buf, sizeof(key_buf), "wifi_ap_%d_ssid", i);
                append_kv(out, sizeof(out), key_buf, "%s", user_config.wifi_aps[i].ssid);

                snprintf(key_buf, sizeof(key_buf), "wifi_ap_%d_password", i);
                append_kv(out, sizeof(out), key_buf, "%s", user_config.wifi_aps[i].password);

                snprintf(key_buf, sizeof(key_buf), "wifi_ap_%d_type", i);
                append_kv(out, sizeof(out), key_buf, "%s", wifi_type_to_string(user_config.wifi_aps[i].connection_type));

                snprintf(key_buf, sizeof(key_buf), "wifi_ap_%d_enabled", i);
                append_kv(out, sizeof(out), key_buf, "%s", user_config.wifi_aps[i].enabled ? "true" : "false");
            }
        }
    }

    UINT bw;
    FRESULT fr = f_write(&file, out, strlen(out), &bw);
    f_close(&file);
    return (fr == FR_OK && bw == strlen(out));
}
