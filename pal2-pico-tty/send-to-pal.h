#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void send_char_to_pal(char ch);
    void send_line_to_pal(const char *line);

#ifdef __cplusplus
}
#endif