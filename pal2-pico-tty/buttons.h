#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

    #include "ssd1306.h"

    const uint PIN_MENU = 12;
    const uint PIN_REWIND = 6;
    const uint PIN_PLAY = 7;
    const uint PIN_FASTFORWARD = 3;
    const uint PIN_RECORD = 2;

    typedef struct
    {
        bool menu;
        bool rewind;
        bool play;
        bool fast_forward;
        bool record;
    } button_state_t;

    void init_buttons(void);

    button_state_t read_buttons_struct(void);
    int menu_select(ssd1306_tty_t *tty, const char **items, int item_count);
    void process_menu(ssd1306_tty_t *tty);

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