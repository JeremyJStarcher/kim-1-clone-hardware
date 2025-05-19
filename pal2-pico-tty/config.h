#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include "ring.h"

    typedef enum
    {
        FILE_STATUS_NONE, /* not yet evaluated / no file          */
        FILE_STATUS_GOOD, /* file present and OK                  */
        FILE_STATUS_ERROR /* any error condition                  */
    } file_status_t;

    typedef enum
    {
        CONNECTION_STATE_NOT_CONNECTED = 10,
        CONNECTION_STATE_NEW_CONNECTION = 11,
        CONNECTION_STATE_CONNECTED = 12,
        CONNECTION_STATE_NEW_DISCONNECT = 13,
    } connection_state_t;

    typedef struct
    {
        bool usb_connected;
        uint8_t oled_address;
        bool tty_mode;
        const char *soft_version;
        file_status_t file_status;
        uint16_t rfcomm_channel_id;
        connection_state_t bt_connected_state;
        connection_state_t usb__connected_state;
    } pal_config_t;

    extern pal_config_t system_config;

    typedef struct
    {
        uint16_t baud;
        uint16_t ch_delay;
        uint16_t line_delay;

        bool use_hard_reset;
        char toggle_char;
        bool force_upper_case;
        bool bs_to_del;
    } user_config_t;

    extern user_config_t user_config;

    extern volatile ring_t tx_ring; /* producer: core-1, consumer: core-0 */
    extern volatile ring_t rx_ring; /* producer: core-0, consumer: core-1 */

    bool load_config_from_sd(void);
    bool save_config_to_sd(void);

#ifdef __cplusplus
}
#endif
