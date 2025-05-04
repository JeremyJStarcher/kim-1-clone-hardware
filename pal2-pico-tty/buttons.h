#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#define PIN_MENU 12
#define PIN_REWIND 6
#define PIN_PLAY 7
#define PIN_FASTFORWARD 3
#define PIN_RECORD 2

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

#ifdef __cplusplus
}
#endif