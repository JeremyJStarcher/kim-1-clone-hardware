#include <stdbool.h>
#include <stdint.h>

#include "kim-reply-parser.h"

kim_reply_parser_t kim_reply_parser;

/* Initialise (or re-initialise) the parser state */
void kim_reply_parser_init(kim_reply_parser_t *p)
{
    *p = (kim_reply_parser_t){0};
}

/* Feed one character; update flags when matches complete */
void kim_reply_parser_feed(kim_reply_parser_t *p, char ch)
{
    /* ---------- detect  ERR  ------------------------------------------- */
    if (!p->err_seen)
    {
        static const char ERR[3] = {'E', 'R', 'R'};

        if (ch == ERR[p->err_idx])
        {
            if (++p->err_idx == 3) /* full match                     */
                p->err_seen = true;
        }
        else
        {
            /* restart if this char could be the beginning of a new match  */
            p->err_idx = (ch == ERR[0]) ? 1 : 0;
        }
    }

    /* ---------- detect  KIM  ------------------------------------------- */
    if (!p->prompt_seen)
    {
        static const char KIM[3] = {'K', 'I', 'M'};

        if (ch == KIM[p->kim_idx])
        {
            if (++p->kim_idx == 3) /* full match                     */
                p->prompt_seen = true;
        }
        else
        {
            p->kim_idx = (ch == KIM[0]) ? 1 : 0;
        }
    }
}
