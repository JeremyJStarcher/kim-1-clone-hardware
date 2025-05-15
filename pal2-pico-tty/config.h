#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

    typedef struct
    {
        bool usb_connected;
        uint8_t oled_address;
        bool tty_mode;
        const char *soft_version;
    } pal_config_t;

    extern pal_config_t system_config;

    typedef struct
    {
        uint32_t baud;
        bool hard_reset;
        char toggle_char;
    } user_config_t;

    extern user_config_t user_config;

    bool load_config_from_sd(void);
    bool save_config_to_sd(void);

#ifdef __cplusplus
}
#endif
