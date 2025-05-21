#include "ssd1306.h"
#include "pal-io.h"
#include "config.h"
#include "edit-settings.h"
#include "buttons.h"

int menu_settings(ssd1306_tty_t *tty)
{

    dmenu_list_t menu = {.count = 0};

    for (size_t i = 0; i < cfg_map_len; i++)
    {
        add_menu_item(&menu, cfg_map[i].key, NULL);
    }

    int ret = process_menu_inner(tty, &menu);

    free_menu(&menu);
    return ret;

    // ssd1306_tty_cls(tty);
    // ssd1306_tty_puts(tty, "SETTINGS");
    // ssd1306_tty_show(tty);

    // while (true)
    // {
    //     button_state_t btn = read_buttons_struct(tty);
    //     if (btn.menu)
    //     {
    //         return SELECT_RETURN_NOACTION;
    //     }
    // }
}
