/* Control Sequence Introducer */
#define ANSI_ESC "\x1b["        /* or "\033[" */

/* ---------- Text attributes ---------- */
#define ANSI_RESET       ANSI_ESC "0m"
#define ANSI_BOLD        ANSI_ESC "1m"
#define ANSI_DIM         ANSI_ESC "2m"
#define ANSI_ITALIC      ANSI_ESC "3m"
#define ANSI_UNDERLINE   ANSI_ESC "4m"
#define ANSI_BLINK       ANSI_ESC "5m"
#define ANSI_REVERSE     ANSI_ESC "7m"
#define ANSI_HIDDEN      ANSI_ESC "8m"
#define ANSI_STRIKE      ANSI_ESC "9m"

/* ---------- Foreground colours (normal) ---------- */
#define ANSI_BLACK       ANSI_ESC "30m"
#define ANSI_RED         ANSI_ESC "31m"
#define ANSI_GREEN       ANSI_ESC "32m"
#define ANSI_YELLOW      ANSI_ESC "33m"
#define ANSI_BLUE        ANSI_ESC "34m"
#define ANSI_MAGENTA     ANSI_ESC "35m"
#define ANSI_CYAN        ANSI_ESC "36m"
#define ANSI_WHITE       ANSI_ESC "37m"

/* ---------- Foreground colours (bright) ---------- */
#define ANSI_BRIGHT_BLACK  ANSI_ESC "90m"
#define ANSI_BRIGHT_RED    ANSI_ESC "91m"
#define ANSI_BRIGHT_GREEN  ANSI_ESC "92m"
#define ANSI_BRIGHT_YELLOW ANSI_ESC "93m"
#define ANSI_BRIGHT_BLUE   ANSI_ESC "94m"
#define ANSI_BRIGHT_MAGENTA ANSI_ESC "95m"
#define ANSI_BRIGHT_CYAN   ANSI_ESC "96m"
#define ANSI_BRIGHT_WHITE  ANSI_ESC "97m"

/* ---------- Background colours (normal) ---------- */
#define ANSI_BG_BLACK    ANSI_ESC "40m"
#define ANSI_BG_RED      ANSI_ESC "41m"
#define ANSI_BG_GREEN    ANSI_ESC "42m"
#define ANSI_BG_YELLOW   ANSI_ESC "43m"
#define ANSI_BG_BLUE     ANSI_ESC "44m"
#define ANSI_BG_MAGENTA  ANSI_ESC "45m"
#define ANSI_BG_CYAN     ANSI_ESC "46m"
#define ANSI_BG_WHITE    ANSI_ESC "47m"

/* ---------- Background colours (bright) ---------- */
#define ANSI_BG_BRIGHT_BLACK  ANSI_ESC "100m"
#define ANSI_BG_BRIGHT_RED    ANSI_ESC "101m"
#define ANSI_BG_BRIGHT_GREEN  ANSI_ESC "102m"
#define ANSI_BG_BRIGHT_YELLOW ANSI_ESC "103m"
#define ANSI_BG_BRIGHT_BLUE   ANSI_ESC "104m"
#define ANSI_BG_BRIGHT_MAGENTA ANSI_ESC "105m"
#define ANSI_BG_BRIGHT_CYAN   ANSI_ESC "106m"
#define ANSI_BG_BRIGHT_WHITE  ANSI_ESC "107m"

/* ---------- Cursor & screen control ---------- */
#define ANSI_CLR_SCREEN    ANSI_ESC "2J"
#define ANSI_CLR_LINE      ANSI_ESC "2K"
#define ANSI_CUR_HOME      ANSI_ESC "H"        /* (row;col default is 1;1) */
#define ANSI_CUR_POS(r,c)  ANSI_ESC #r ";" #c "H"  /* usage: printf(CUR_POS(10,5)); */
#define ANSI_CUR_SAVE      ANSI_ESC "s"
#define ANSI_CUR_RESTORE   ANSI_ESC "u"
