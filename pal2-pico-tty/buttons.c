#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "stdlib.h"
#include "stdio.h"

#include "buttons.h"

void init_buttons(void)
{
    // Array of all button pins
    const uint8_t button_pins[] = {
        PIN_MENU,
        PIN_REWIND,
        PIN_PLAY,
        PIN_FASTFORWARD,
        PIN_RECORD};

    for (int i = 0; i < sizeof(button_pins) / sizeof(button_pins[0]); ++i)
    {
        uint pin = button_pins[i];
        gpio_init(pin);             // Initialize GPIO
        gpio_set_dir(pin, GPIO_IN); // Set as input
        gpio_pull_up(pin);          // Enable internal pull-up
    }
}

button_state_t read_buttons_struct(void)
{
    button_state_t state;

    state.menu = !gpio_get(PIN_MENU);
    state.rewind = !gpio_get(PIN_REWIND);
    state.play = !gpio_get(PIN_PLAY);
    state.fast_forward = !gpio_get(PIN_FASTFORWARD);
    state.record = !gpio_get(PIN_RECORD);

    return state;
}

int menu_select(ssd1306_tty_t *tty, const char **items, int item_count)
{
    int MAX_VISIBLE_ITEMS = tty->height;

    int selected_index = 0;
    int top_index = 0;

    while (true)
    {
        // Clear the display buffer
        ssd1306_clear(tty->ssd1306);

        // Display the visible portion of the menu
        for (int i = 0; i < MAX_VISIBLE_ITEMS; ++i)
        {
            int item_idx = top_index + i;
            if (item_idx >= item_count)
                break;

            char line[128];
            if (item_idx == selected_index)
            {
                snprintf(line, sizeof(line), "> %s", items[item_idx]);
            }
            else
            {
                snprintf(line, sizeof(line), "  %s", items[item_idx]);
            }

            ssd1306_tty_puts(tty, "\n", 0);
            ssd1306_tty_puts(tty, line, 0);
        }

        ssd1306_tty_show(tty);

        // Read button states
        button_state_t btn = read_buttons_struct();

        // Handle button presses
        if (btn.rewind)
        {
            if (selected_index > 0)
            {
                --selected_index;
                if (selected_index < top_index)
                {
                    --top_index;
                }
            }
        }
        else if (btn.fast_forward)
        {
            if (selected_index < item_count - 1)
            {
                ++selected_index;
                if (selected_index >= top_index + MAX_VISIBLE_ITEMS)
                {
                    ++top_index;
                }
            }
        }
        else if (btn.play)
        {
            return selected_index;
        }
        else if (btn.menu)
        {
            return -1;
        }

        // Debounce delay
        // sleep_ms(150);
    }
}

void process_menu(ssd1306_tty_t *tty)
{

    const char *menu_items[] = {
        "Send File",
        "Settings",
        "Baud Rate",
        "Character Delay",
        "Line Delay",
        "Option 6",
        "Option 7",
        "Option 8",
        "Option 9",
        "Option 10"};

    int selected = menu_select(tty, menu_items, sizeof(menu_items) / sizeof(menu_items[0]));

    ssd1306_clear(tty->ssd1306);

    if (selected >= 0)
    {
        // Handle the selected menu item
        printf("Selected: %s\n", menu_items[selected]);
    }
    else
    {
        // Handle menu exit
        printf("Menu closed.\n");
    }
}