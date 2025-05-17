#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    #include "ssd1306.h"

    void disable_tty_mode(ssd1306_tty_t *tty);
    void enable_tty_mode(ssd1306_tty_t *tty);
    void reset_pal(ssd1306_tty_t *tty);
    int get_edges(int pin, int window_us);
    int user_getchar(void);


    void upload_char_to_pal(char ch);
    void upload_line_to_pal(const char *line);
#ifdef __cplusplus
}
#endif
