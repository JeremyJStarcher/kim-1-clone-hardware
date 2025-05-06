#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "ssd1306.h"

    const uint8_t PIN_MENU = 12;
    const uint8_t PIN_REWIND = 6;
    const uint8_t PIN_PLAY = 7;
    const uint8_t PIN_FASTFORWARD = 3;
    const uint8_t PIN_RECORD = 2;

    const uint8_t BUTTON_STATE_NONE = 0;
    const uint8_t BUTTON_STATE_PRESSED = 1;
    const uint8_t BUTTON_STATE_REPEAT = 2;

    typedef struct
    {
        uint8_t menu;
        uint8_t rewind;
        uint8_t play;
        uint8_t fast_forward;
        uint8_t record;
        bool any;
    } button_state_t;

    // typedef const char *menu_item_t;
    // typedef menu_item_t *menu_list_t;
    typedef void (*menu_callback_t)(ssd1306_tty_t *tty);

    typedef struct
    {
        const char *label;
        menu_callback_t callback; // May be NULL
    } menu_item_t;

    typedef const menu_item_t *menu_list_t;

    void init_buttons(void);

    button_state_t read_buttons_struct(void);

    int menu_select(ssd1306_tty_t *tty, menu_list_t items, int item_count);
    int process_menu(ssd1306_tty_t *tty);

#ifdef __cplusplus
}
#endif

/*
State machine

* Idle
* Menu
  - Send File
    + Directory List
      . Change Directory (up or down)
      . Upload file
    + Settings
      . Baud
         - 300
         - 2400
         - 9600
      . Character Delay
         - Enter Number
      . Line Delay
          - Enter Delay


*/