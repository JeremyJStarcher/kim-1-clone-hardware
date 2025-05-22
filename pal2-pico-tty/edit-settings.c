#include <string.h>
#include <stdlib.h>

#include "ssd1306.h"
#include "pal-io.h"
#include "config.h"
#include "edit-settings.h"
#include "buttons.h"

#include <string.h> // for strtok, strdup
#include <stdlib.h> // for strdup

static void add_tabbed_str_to_menu(ssd1306_tty_t *tty, dmenu_list_t *menu, char *mutable_options)
{

    if (mutable_options != NULL)
    {
        char *token = strtok(mutable_options, "\t");
        while (token != NULL)
        {
            add_menu_item(menu, token, NULL); // NULL callback
            token = strtok(NULL, "\t");
        }
    }

    return;
}

static int show_saved(ssd1306_tty_t *tty)
{

    ssd1306_tty_cls(tty);
    ssd1306_tty_puts(tty, "SAVED\r\n");
    ssd1306_tty_puts(tty, "\r\n");
    ssd1306_tty_puts(tty, "TOGGLE TTY MODE\r\n");
    ssd1306_tty_puts(tty, "TO APPLY\r\n");
    ssd1306_tty_show(tty);

    while (true)
    {
        button_state_t btn = read_buttons_struct(tty);
        if (btn.any)
        {
            return SELECT_RETURN_NOACTION;
        }
    }
}

static int menu_save_config(ssd1306_tty_t *tty, void *_item)
{
    save_config_to_sd();
    show_saved(tty);
    return SELECT_RETURN_CLOSE_ALL;
}

static int menu_none(ssd1306_tty_t *tty, void *_item)
{

    ssd1306_tty_cls(tty);
    ssd1306_tty_puts(tty, "NOT YET");
    ssd1306_tty_show(tty);

    while (true)
    {
        button_state_t btn = read_buttons_struct(tty);
        if (btn.menu)
        {
            return SELECT_RETURN_NOACTION;
        }
    }
}

static int menu_list(ssd1306_tty_t *tty, void *_item)
{
    dmenu_item_t *item = (dmenu_item_t *)_item;

    dmenu_list_t menu = {.count = 0};

    char *mutable_options = strdup(item->config_entry->validations); // Make a writable copy

    add_tabbed_str_to_menu(tty, &menu, mutable_options);

    int ret = process_menu_inner(tty, &menu);
    if (ret > -1)
    {
        char eq[] = " = ";
        const char *a = item->config_entry->key;
        const char *c = menu.items[ret].label;

        size_t len = strlen(a) + strlen(eq) + strlen(c) + 1; // +1 for null terminator
        char *result = malloc(len);

        if (result == NULL)
        {
            u_printf("OUT OF MEMORY IN menu_list\r\n");
        }

        strcpy(result, a);  // Copy first string
        strcat(result, eq); // Append second
        strcat(result, c);  // Append third

        parse_config_line(result);
        free(result);
    }

    free(mutable_options);
    free_menu(&menu);
    return ret;
}

static dmenu_item_t *add_config_menu_item(
    dmenu_list_t *menu,
    const char *label,
    dmenu_callback_t callback,
    config_entry_t *config_entry)
{
    if (menu->count >= MAX_MENU_ITEMS)
        return NULL; // handle overflow
    menu->items[menu->count].label = label;
    menu->items[menu->count].callback = callback;
    menu->items[menu->count].is_dir = false;
    menu->items[menu->count].config_entry = config_entry;
    menu->count++;

    return &menu->items[menu->count - 1];
}

int menu_settings2(ssd1306_tty_t *tty, void *_item)
{

    dmenu_list_t menu = {.count = 0};

    for (size_t i = 0; i < cfg_map_len; i++)
    {
        dmenu_callback_t callback = NULL;

        switch (cfg_map[i].menu_type)
        {
        case MENU_TYPE_NONE:
            callback = &menu_none;
            break;
        case MENU_TYPE_LIST:
            callback = &menu_list;
            break;
        default:
            callback = &menu_none;
        }

        add_config_menu_item(&menu, cfg_map[i].key, callback, &cfg_map[i]);
    }

    add_config_menu_item(&menu, "**SAVE**", menu_save_config, NULL);

    int ret = process_menu_inner(tty, &menu);
    free_menu(&menu);
    return ret;
}

int menu_settings(ssd1306_tty_t *tty, void *_item)
{
    while (true)
    {
        int ret = menu_settings2(tty, _item);
        if (ret == SELECT_RETURN_NOACTION)
        {
            break;
        }
    }
}
