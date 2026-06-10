/* spy80_ansi.c - ANSI escape parser for 80-col bitmap spy view.
 * Lives in MAIN code (default sections) so it stays resident while the MSGS
 * overlay is loaded — the spy must keep rendering during message-base browsing. */
#include "bbs/spy80.h"
#include "bbs/types.h"
#include <string.h>

/* States */
#define ST_NORMAL   0
#define ST_ESC      1
#define ST_BRACKET  2
#define ST_PARAM    3

/* ANSI -> VIC color table (index by ANSI 30-37; bright 90-97 adds 8) */
static const u8 s_ansi_vic[16] = {
    0,  /* 30: black       -> VIC 0  */
    2,  /* 31: red         -> VIC 2  */
    5,  /* 32: green       -> VIC 5  */
    7,  /* 33: yellow      -> VIC 7  */
    6,  /* 34: blue        -> VIC 6  */
    4,  /* 35: magenta     -> VIC 4  */
    3,  /* 36: cyan        -> VIC 3  */
    1,  /* 37: white       -> VIC 1  */
    11, /* 90: dark grey   -> VIC 11 */
    10, /* 91: lt. red     -> VIC 10 */
    13, /* 92: lt. green   -> VIC 13 */
    7,  /* 93: lt. yellow  -> VIC 7  */
    14, /* 94: lt. blue    -> VIC 14 */
    4,  /* 95: lt. magenta -> VIC 4  */
    3,  /* 96: lt. cyan    -> VIC 3  */
    1,  /* 97: lt. white   -> VIC 1  */
};

/* Module state */
static u8  s_state  = ST_NORMAL;
static u8  s_col    = 0;
static u8  s_row    = 0;
static u8  s_color  = 1;       /* VIC color index (1=white) */
static u8  s_p[4];             /* up to 4 CSI parameters */
static u8  s_pn    = 0;        /* number of params collected */
static u8  s_pcur  = 0;        /* current param accumulator */

/* Forward-declared — implemented in spy80.c */
extern void spy80_render_char(u8 col, u8 row, u8 ch, u8 vic_color);

/* CP437 upper-half -> displayable ASCII approximation (compact). */
static u8 cp437_remap(u8 ch)
{
    if (ch >= 0x20u && ch <= 0x7Eu) return ch;   /* ASCII pass-through */
    if (ch == 0xB3u || ch == 0xBAu) return '|';  /* vertical lines */
    if (ch == 0xC4u || ch == 0xCDu) return '-';  /* horizontal lines */
    if (ch >= 0xB0u && ch <= 0xB2u) return '#';  /* shading blocks */
    if (ch >= 0xB4u && ch <= 0xDFu) return '+';  /* corners/tees/cross/blocks */
    return ' ';
}

static void flush_param(void)
{
    if (s_pn < 4) { s_p[s_pn++] = s_pcur; }
    s_pcur = 0;
}

static void handle_sgr(void)
{
    u8 i;
    if (s_pn == 0) { s_color = 1; return; }   /* bare ESC[m = reset */
    for (i = 0; i < s_pn; i++) {
        u8 v = s_p[i];
        if (v == 0) {
            s_color = 1;           /* reset -> white */
        } else if (v >= 30 && v <= 37) {
            s_color = s_ansi_vic[v - 30];
        } else if (v >= 90 && v <= 97) {
            s_color = s_ansi_vic[v - 90 + 8];
        }
        /* 40-47 (bg): ignored — bitmap bg always black */
    }
}

static void handle_action(u8 action)
{
    u8 n;
    switch (action) {
        case 'A': /* cursor up */
            n = (s_pn > 0 && s_p[0] > 0) ? s_p[0] : 1;
            s_row = (s_row >= n) ? (u8)(s_row - n) : 0;
            break;
        case 'B': /* cursor down */
            n = (s_pn > 0 && s_p[0] > 0) ? s_p[0] : 1;
            s_row = (s_row + n < SPY80_CONTENT_ROWS) ? (u8)(s_row + n) : (u8)(SPY80_CONTENT_ROWS - 1);
            break;
        case 'C': /* cursor right */
            n = (s_pn > 0 && s_p[0] > 0) ? s_p[0] : 1;
            s_col = (s_col + n < SPY80_COLS) ? (u8)(s_col + n) : (u8)(SPY80_COLS - 1);
            break;
        case 'D': /* cursor left */
            n = (s_pn > 0 && s_p[0] > 0) ? s_p[0] : 1;
            s_col = (s_col >= n) ? (u8)(s_col - n) : 0;
            break;
        case 'H': /* cursor home / absolute */
        case 'f':
            s_row = (s_pn > 0 && s_p[0] > 0) ? (u8)(s_p[0] - 1) : 0;
            s_col = (s_pn > 1 && s_p[1] > 0) ? (u8)(s_p[1] - 1) : 0;
            if (s_row >= SPY80_CONTENT_ROWS) s_row = SPY80_CONTENT_ROWS - 1;
            if (s_col >= SPY80_COLS) s_col = SPY80_COLS - 1;
            break;
        case 'J': /* erase display */
            if (s_pn == 0 || s_p[0] == 2) spy80_clear();
            break;
        case 'm': /* SGR */
            handle_sgr();
            break;
        default:
            break; /* unhandled — consumed silently */
    }
}

void spy80_ansi_reset(void)
{
    s_state = ST_NORMAL;
    s_col   = 0; s_row   = 0;
    s_color = 1; s_pn    = 0;
    s_pcur  = 0;
}

void spy80_ansi_feed(u8 ch)
{
    switch (s_state) {
        case ST_NORMAL:
            if (ch == 0x1Bu) {            /* ESC */
                s_state = ST_ESC;
            } else if (ch == 0x0Du) {     /* CR */
                s_col = 0;
            } else if (ch == 0x0Au) {     /* LF */
                if (s_row < SPY80_CONTENT_ROWS - 1) s_row++;
                else spy80_scroll();      /* at bottom: scroll, cursor stays */
            } else if (ch >= 0x20u) {
                u8 disp = cp437_remap(ch);
                if (disp >= 0x20u && disp <= 0x7Eu) {
                    spy80_render_char(s_col, s_row, disp, s_color);
                }
                if (++s_col >= SPY80_COLS) {
                    s_col = 0;
                    if (s_row < SPY80_CONTENT_ROWS - 1) s_row++;
                    else spy80_scroll();
                }
            }
            break;

        case ST_ESC:
            s_state = (ch == '[') ? ST_BRACKET : ST_NORMAL;
            break;

        case ST_BRACKET:
            s_pn = 0; s_pcur = 0;
            memset(s_p, 0, sizeof(s_p));
            if (ch >= '0' && ch <= '9') {
                s_pcur = (u8)(ch - '0');
                s_state = ST_PARAM;
            } else if (ch == ';') {
                flush_param();
                s_state = ST_PARAM;
            } else {
                s_state = ST_NORMAL;
                handle_action(ch);
            }
            break;

        case ST_PARAM:
            if (ch >= '0' && ch <= '9') {
                s_pcur = (u8)(s_pcur * 10 + (ch - '0'));
            } else if (ch == ';') {
                flush_param();
            } else {
                flush_param();
                s_state = ST_NORMAL;
                handle_action(ch);
            }
            break;
    }
}
