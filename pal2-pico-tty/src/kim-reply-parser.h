/* ----------------------------------------------------------------
 *  Parser state for the KIM reply stream
 * ----------------------------------------------------------------
 *  This parser is used to detect the "ERR" and "KIM" strings in the
 *  reply stream from the KIM-1. It is not a full parser, but just
 *  enough to detect these two strings.
 *
 *  The parser is stateful, so it needs to be initialised before use.
 * ---------------------------------------------------------------- */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

    typedef struct
    {
        bool err_seen;    /* “ERR” has appeared              */
        bool prompt_seen; /* “KIM” has appeared              */
        uint8_t err_idx;  /* progress in "ERR"  (0-3)        */
        uint8_t kim_idx;  /* progress in "KIM"  (0-3)        */
    } kim_reply_parser_t;

    extern kim_reply_parser_t kim_reply_parser;

    void kim_reply_parser_init(kim_reply_parser_t *p);
    void kim_reply_parser_feed(kim_reply_parser_t *p, char ch);

#ifdef __cplusplus
}
#endif
