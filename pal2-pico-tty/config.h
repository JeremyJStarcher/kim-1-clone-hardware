#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

    typedef enum
    {
        FILE_STATUS_NONE, /* not yet evaluated / no file          */
        FILE_STATUS_GOOD, /* file present and OK                  */
        FILE_STATUS_ERROR /* any error condition                  */
    } file_status_t;

    typedef struct
    {
        bool usb_connected;
        uint8_t oled_address;
        bool tty_mode;
        const char *soft_version;
        file_status_t file_status;
    } pal_config_t;

    extern pal_config_t system_config;

    typedef struct
    {
        uint16_t baud;
        uint16_t ch_delay;
        uint16_t line_delay;

        bool use_hard_reset;
        char toggle_char;
    } user_config_t;

    extern user_config_t user_config;

    bool load_config_from_sd(void);
    bool save_config_to_sd(void);

#ifdef __cplusplus
}
#endif
