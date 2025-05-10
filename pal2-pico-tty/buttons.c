#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "buttons.h"
#include "ssd1306.h"
#include "sd-card/sd-card.h"
#include "proj_hw.h"

static const int SHORT_DELAY      = 20;
static const int LONG_DELAY       = 200;

static const int PROGRESS_STEPS   = 100; // granularity: 1%
static const int BAR_WIDTH_CHARS  = 20;  // ########··············


static const char DIR_SYMBOLS[] = "[]";

/* ----------------------------------------------------------------
 *  Per‑build tuning — adjust to taste
 * ---------------------------------------------------------------- */
#define DEBOUNCE_US (20 * 1000)      //  mechanical bounce mask
#define REPEAT_DELAY_US (500 * 1000) //  before first repeat
#define REPEAT_RATE_US (150 * 1000)  // 150 ms: between repeats

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

static void oled_progress(ssd1306_tty_t *tty,
                          uint32_t sent, uint32_t total,
                          const char *file_name)
{
    if (total == 0) total = 1;                 /* prevent /0               */

    uint32_t pct = (sent * 100) / total;             /* 0–100       */
    uint32_t filled = (pct * BAR_WIDTH_CHARS) / 100; /* 0–BAR_WIDTH */

    static char line[10];

    sprintf(line, "%3u%%", (unsigned)pct);

    ssd1306_draw_string(tty->ssd1306, 1, 1, 3, line);
    ssd1306_draw_string(tty->ssd1306, 4, 25, 1, file_name);

    ssd1306_show(tty->ssd1306);
}

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

    event.any = false;

    if (event.menu || event.rewind || event.play || event.fast_forward || event.record)
    {
        event.any = true;
    }

    return event; // 1 = “new press” or “repeat pulse”, 0 = idle
}

int menu_select(ssd1306_tty_t *tty, dmenu_list_t *menu)
{
    size_t item_count = menu->count;

    int MAX_VISIBLE_ITEMS = MIN(tty->height, item_count);

    int selected_index = 0;
    int top_index = 0;
    bool do_redraw = true;

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

            char line[MAX_PATH_LEN];
            if (item_idx == selected_index)
            {
                snprintf(line, sizeof(line), "> %s", menu->items[item_idx].label);
            }
            else
            {
                snprintf(line, sizeof(line), "  %s", menu->items[item_idx].label);
            }

            if (menu->items[item_idx].is_dir)
            {
                line[1] = DIR_SYMBOLS[0];
                char s[] = {DIR_SYMBOLS[1], 0};
                strncat(line, s, MAX_PATH_LEN - 1);
            }

            // Prevent the line from running off the screen
            line[tty->width] = 0;

            if (i != 0)
            {
                ssd1306_tty_puts(tty, "\n");
            }
            ssd1306_tty_puts(tty, line);
        }

        if (do_redraw)
        {
            ssd1306_tty_show(tty);
            do_redraw = false;
        }

        // Read button states
        button_state_t btn = read_buttons_struct();

        // Handle button presses
        if (btn.rewind)
        {
            do_redraw = true;
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
            do_redraw = true;
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
    ssd1306_tty_puts(tty, "ABOUT");
    ssd1306_tty_show(tty);

    while (true)
    {
        button_state_t btn = read_buttons_struct();
        if (btn.menu)
        {
            return;
        }
    }
}

void tree_to_menu(DirEntry *node, dmenu_list_t *menu, int level)
{
    while (node)
    {
        dmenu_item_t *menu_item = add_menu_item(menu, node->name, NULL);
        if (node->is_dir)
        {
            menu_item->is_dir = true;
            tree_to_menu(node->children, menu, level + 1);
        }
        node = node->sibling;
    }
}

static int cmp_items(const void *a, const void *b)
{
    const dmenu_item_t *ia = a, *ib = b;
    return strcasecmp(ia->label, ib->label);
}

void path_up(char *path)
{
    if (!path || !*path) /* NULL or empty → nothing to do        */
        return;

    /* 1. Trim a trailing '/' unless the path is exactly "X:/"               */
    size_t len = strlen(path);
    if (len > 3 && path[len - 1] == '/')
    {
        path[--len] = '\0'; /* drop the trailing slash              */
    }

    /* 2. Find the last slash that separates components                      */
    char *slash = strrchr(path, '/');
    if (!slash) /* no slash at all → leave string       */
        return;

    /* 3. If we are deeper than root ("X:/"), cut there; else keep "X:/"     */
    if (slash > path + 2)
    { /* > "X:/"  → cut component             */
        *slash = '\0';
    }
    else
    {                   /* already at "X:/" → ensure proper NUL */
        path[3] = '\0'; /* keeps "X:/" exactly                  */
    }
}

void send_file(ssd1306_tty_t *tty, const char *dir, const char *file_name)
{
#define LINE_BUF_LEN 255

    char full_file_name[MAX_PATH_LEN];
    FIL fp;
    FRESULT fr;
    UINT br;
    char line[LINE_BUF_LEN];

    size_t used = snprintf(full_file_name, MAX_PATH_LEN, "%s%s%s",
                           dir,
                           (dir[0] && dir[strlen(dir) - 1] != '/') ? "/" : "",
                           file_name);

    printf("FULL FILE NAME %s\n", full_file_name);

    fr = f_open(&fp, full_file_name, FA_READ);
    if (fr != FR_OK)
    {
        //         return fr;
    }

    DWORD sz = f_size(&fp); /* Constant-time size fetch          */
    printf("File: %s  (%lu bytes)\n\n", full_file_name, (unsigned long)sz);

    const DWORD total = f_size(&fp);
    uint32_t last_step = 0; /* last % drawn       */

    oled_progress(tty, 0, total, file_name);

    while (f_gets(line, sizeof line, &fp))
    {
        size_t n = strlen(line);


        for (size_t i = 0; i <= n; i++)
        {
            char ch = line[i];
            uart_putc_raw(PAL_UART, (uint8_t)line[i]);
            if (ch == '\r' || ch == '\n')
            {
                sleep_ms(LONG_DELAY);
            }
            else
            {
                sleep_ms(SHORT_DELAY);
            }
        }
        if (n && line[n - 1] != '\n')
        {
            uart_putc_raw(PAL_UART, (uint8_t)'\n');
            sleep_ms(LONG_DELAY);
        }

        uint32_t sent = f_tell(&fp); /* bytes already read   */
        uint32_t step = (sent * PROGRESS_STEPS) / total;

        if (step != last_step)
        { /* crossed 1 % boundary */
            last_step = step;
            oled_progress(tty, sent, total, file_name);
            /* term_progress(sent, total);     <-- enable if no OLED   */
        }
    }

    oled_progress(tty, total, total, file_name);
    f_close(&fp);
}

void menu_tty_up(ssd1306_tty_t *tty)
{
    dmenu_list_t menu = {.count = 0};
    DirEntry *root = NULL;
    char current_dir[MAX_PATH_LEN];

    strncpy(current_dir, DRIVE_PATH PTP_PATH, MAX_PATH_LEN);

    while (true)
    {

        add_menu_item(&menu, "..", NULL);
        menu.items[0].is_dir = true;

        printf("CURRENT DIRECTORY %s\n", current_dir);

        FRESULT res = build_tree(current_dir, &root, false);

        if (res != FR_OK)
        {
            ssd1306_tty_cls(tty);
            ssd1306_tty_printf(tty, "ERROR# %d", res);
            ssd1306_tty_show(tty);
            while (1)
            {
                ;
            }
        }

        tree_to_menu(root, &menu, 0);

        /* call once after populating `menu` */
        qsort(menu.items, menu.count, sizeof(dmenu_item_t), cmp_items);

        int ret = process_menu_inner(tty, &menu);
        if (ret == -1)
        {
            free_tree(root);
            free_menu(&menu);
            return;
        }

        dmenu_item_t item = menu.items[ret];

        if (!item.is_dir)
        {
            send_file(tty, current_dir, item.label);
        }
        else
        {
            if (strcmp(item.label, "..") == 0)
            {
                path_up(current_dir);
            }
            else
            {
                if (current_dir[strlen(current_dir) - 1] != '/')
                {
                    snprintf(current_dir + strlen(current_dir),
                             MAX_PATH_LEN - strlen(current_dir), /* space still available      */
                             "%s", "/");
                }

                snprintf(current_dir + strlen(current_dir),
                         MAX_PATH_LEN - strlen(current_dir), /* space still available      */
                         "%s", item.label);
            }
        }

        free_tree(root);
        free_menu(&menu);
    }
}

int process_menu_inner(ssd1306_tty_t *tty, dmenu_list_t *menu)
{
    ssd1306_tty_set_scale(tty, 1);

    while (true)
    {
        int selected = menu_select(tty, menu);

        ssd1306_tty_cls(tty);

        if (selected >= 0)
        {
            dmenu_item_t item = menu->items[selected];
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

int process_menu(ssd1306_tty_t *tty)
{
    dmenu_list_t menu = {.count = 0};

    // ✅ Populate menu
    add_menu_item(&menu, "ABOUT", menu_about);
    add_menu_item(&menu, "TTY UP", menu_tty_up);
    add_menu_item(&menu, "Option 1", NULL);
    add_menu_item(&menu, "Option 2", NULL);

    int ret = process_menu_inner(tty, &menu);

    free_menu(&menu);
    return ret;
}

dmenu_item_t *add_menu_item(dmenu_list_t *menu, char *label, dmenu_callback_t callback)
{
    if (menu->count >= MAX_MENU_ITEMS)
        return NULL; // handle overflow
    menu->items[menu->count].label = label;
    menu->items[menu->count].callback = callback;
    menu->items[menu->count].is_dir = false;
    menu->count++;

    return &menu->items[menu->count - 1];
}

void free_menu(dmenu_list_t *menu)
{
    for (size_t i = 0; i < menu->count; i++)
    {
        menu->items[i].label = NULL;
        menu->items[i].is_dir = false;
    }
    menu->count = 0;
}