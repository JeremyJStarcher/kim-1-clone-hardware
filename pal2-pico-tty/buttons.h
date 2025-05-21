#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "ssd1306.h"

  static const int SELECT_RETURN_CLOSE = -1;
  static const int SELECT_RETURN_NOACTION = -2;
  static const int SELECT_RETURN_CLOSE_ALL = -3;

  typedef enum
  {
    FILE_TYPE_PTP,        /* Proprietary Transfer Protocol           */
    FILE_TYPE_PLAIN_TEXT, /* Human-readable text                     */

    FILE_TYPE_COUNT /* Always keep this as the last item.      */
  } file_type_t;

  extern const uint8_t PIN_MENU;
  extern const uint8_t PIN_REWIND;
  extern const uint8_t PIN_PLAY;
  extern const uint8_t PIN_FASTFORWARD;
  extern const uint8_t PIN_RECORD;

  typedef enum
  {
    BUTTON_STATE_NONE = 0,
    BUTTON_STATE_PRESSED = 1,
    BUTTON_STATE_REPEAT = 2
  } button_state_enum_t;

  typedef struct
  {
    button_state_enum_t menu;
    button_state_enum_t rewind;
    button_state_enum_t play;
    button_state_enum_t fast_forward;
    button_state_enum_t record;
    bool any;
  } button_state_t;

#define MAX_MENU_ITEMS 32 // or whatever fits in memory safely

  typedef int (*dmenu_callback_t)(ssd1306_tty_t *tty);

  typedef struct
  {
    const char *label;
    dmenu_callback_t callback;
    bool is_dir; // A prefix symbol to show
  } dmenu_item_t;

  typedef struct
  {
    dmenu_item_t items[MAX_MENU_ITEMS];
    size_t count;
  } dmenu_list_t;

  void init_buttons(void);

  button_state_t read_buttons_struct(ssd1306_tty_t *tty);

  dmenu_item_t *add_menu_item(dmenu_list_t *menu, const char *label, dmenu_callback_t callback);
  int menu_select(ssd1306_tty_t *tty, dmenu_list_t *menu);
  void free_menu(dmenu_list_t *menu);
  int process_menu(ssd1306_tty_t *tty);
  int process_menu_inner(ssd1306_tty_t *tty, dmenu_list_t *menu);
  void reset_screen_timer(void);

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