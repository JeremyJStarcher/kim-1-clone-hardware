#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "stdlib.h"
#include "stdio.h"

#include "buttons.h"

/* ----------------------------------------------------------------
 *  Per‑build tuning — adjust to taste
 * ---------------------------------------------------------------- */
#define DEBOUNCE_US (20 * 1000)     //  mechanical bounce mask
#define REPEAT_DELAY_US (500 * 1000) //  before first repeat
#define REPEAT_RATE_US (150 * 1000) // 150 ms: between repeats

/* ----------------------------------------------------------------
 *  Internal bookkeeping for each physical key
 * ---------------------------------------------------------------- */
typedef struct
{
    bool stable;          // debounced logical level (0 = up, 1 = down)
    bool last_raw;        // last raw sample
    uint64_t t_last_edge; // when raw last *changed*
    uint64_t t_last_emit; // when we last produced a pulse
    bool is_repeat;
} btn_fsm_t;

static btn_fsm_t btn_fsm[5]; // one entry per button, index order = pins[]

// Array of all button pins
static const uint8_t button_pins[] = {
    PIN_MENU,
    PIN_REWIND,
    PIN_PLAY,
    PIN_FASTFORWARD,
    PIN_RECORD};

////////////////////////////////

///////////////////

void init_buttons(void)
{

    for (int i = 0; i < sizeof(button_pins) / sizeof(button_pins[0]); ++i)
    {
        uint pin = button_pins[i];
        gpio_init(pin);             // Initialize GPIO
        gpio_set_dir(pin, GPIO_IN); // Set as input
        gpio_pull_up(pin);          // Enable internal pull-up
    }

    uint64_t now = time_us_64();
    for (size_t i = 0; i < sizeof(button_pins) / sizeof(button_pins[0]); ++i)
    {
        btn_fsm[i].stable = false; // released
        btn_fsm[i].last_raw = false;
        btn_fsm[i].t_last_edge = now;
        btn_fsm[i].t_last_emit = 0;
    }
}

/* ----------------------------------------------------------------
 *  read_buttons_struct()
 *  – non‑blocking debounce + repeat generator
 *    Returns a one‑shot “event” pulse for each key.
 * ---------------------------------------------------------------- */
button_state_t read_buttons_struct(void)
{
    button_state_t event = {0};
    uint64_t now = time_us_64();

    for (size_t i = 0; i < 5; ++i)
    {

        /* 1. Sample hardware (active‑low) */
        bool raw = !gpio_get(button_pins[i]);

        /* 2. Edge detection for debounce */
        if (raw != btn_fsm[i].last_raw)
        {
            btn_fsm[i].last_raw = raw;
            btn_fsm[i].t_last_edge = now; // reset debounce timer
        }

        /* 3. If stable longer than DEBOUNCE_US, accept new state */
        if ((now - btn_fsm[i].t_last_edge) >= DEBOUNCE_US &&
            raw != btn_fsm[i].stable)
        {

            btn_fsm[i].stable = raw;
            btn_fsm[i].t_last_emit = now;
            btn_fsm[i].is_repeat = false;

            if (raw)
            { // rising edge → emit pulse
                if (i == 0)
                {
                    printf("MENU PRESSED\n");
                    event.menu = BUTTON_STATE_PRESSED;
                }
                if (i == 1)
                    event.rewind = BUTTON_STATE_PRESSED;
                if (i == 2)
                    event.play = BUTTON_STATE_PRESSED;
                if (i == 3)
                    event.fast_forward = BUTTON_STATE_PRESSED;
                if (i == 4)
                    event.record = BUTTON_STATE_PRESSED;
            }
        }

        /* 4. Held‑down state → handle repeats */
        if (btn_fsm[i].stable)
        {
            uint64_t gap = btn_fsm[i].is_repeat ? REPEAT_RATE_US : REPEAT_DELAY_US;

            if ((now - btn_fsm[i].t_last_emit) >= gap) // enough time since first press
            {
                btn_fsm[i].is_repeat = true;
                btn_fsm[i].t_last_emit = now;

                if (i == 0)
                    event.menu = BUTTON_STATE_REPEAT;
                if (i == 1)
                    event.rewind = BUTTON_STATE_REPEAT;
                if (i == 2)
                    event.play = BUTTON_STATE_REPEAT;
                if (i == 3)
                    event.fast_forward = BUTTON_STATE_REPEAT;
                if (i == 4)
                    event.record = BUTTON_STATE_REPEAT;
            }
        }
    }

    return event; // 1 = “new press” or “repeat pulse”, 0 = idle
}

int menu_select(ssd1306_tty_t *tty, menu_list_t items, int item_count)
{
    int MAX_VISIBLE_ITEMS = MIN(tty->height, item_count);

    int selected_index = 0;
    int top_index = 0;

    printf("MAX_VISIBLE_ITEMS %d\n", MAX_VISIBLE_ITEMS);

    while (true)
    {
        // Clear the display buffer
        ssd1306_tty_cls(tty);

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
            // Prevent the line from running off the screen
            line[tty->width] = 0;

            if (i != 0)
            {
                ssd1306_tty_puts(tty, "\n", 0);
            }
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
        else if (btn.play == BUTTON_STATE_PRESSED)
        {
            return selected_index;
        }
        else if (btn.menu == BUTTON_STATE_PRESSED)
        {
            return -1;
        }

        // Debounce delay
        // sleep_ms(150);
    }
}

void menu_about(ssd1306_tty_t *tty)
{
    ssd1306_tty_cls(tty);
    ssd1306_tty_puts(tty, "ABOUT", 0);
    ssd1306_tty_show(tty);

    while (true)
    {
        button_state_t btn = read_buttons_struct();
        if (btn.menu)
        {
            return;
        }
        sleep_ms(100);
    }
}

int process_menu(ssd1306_tty_t *tty)
{

    menu_item_t menu_items[] = {
        // { "Send File",      send_file_callback },
        // { "Settings",       settings_callback },
        // { "Baud Rate",      baud_rate_callback },
        // { "Character Delay", NULL },
        {"ABOUT", menu_about},
        {"Op", NULL},
        {"Opt", NULL},
        {"Option 8", NULL},
        {"Option 9", NULL},
        {"Option 10", NULL},
        {"Line Delay", NULL},
        {"Option 6", NULL},
        {"Option 7", NULL},
        {"Option 8", NULL},
        {"Option 9", NULL},
        {"Option 10", NULL},
        {"Line Delay", NULL},
        {"Option 6", NULL},
        {"Option 7", NULL},
        {"Option 8", NULL},
        {"Option 9", NULL},
        {"Option 10", NULL},
    };

    ssd1306_tty_set_scale(tty, 2);

    while (true)
    {
        int selected = menu_select(tty, menu_items, sizeof(menu_items) / sizeof(menu_items[0]));

        ssd1306_clear(tty->ssd1306);

        if (selected >= 0)
        {
            menu_item_t item = menu_items[selected];
            if (item.callback != NULL)
            {
                item.callback(tty);
            }
            else
            {
                return selected;
            }
        }
        else
        {
            return selected;
        }
    }
}