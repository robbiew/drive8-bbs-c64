/* bbs/spy80.h - 80-column hires bitmap spy view API. */
#ifndef INCLUDE_BBS_SPY80_H
#define INCLUDE_BBS_SPY80_H

#include "bbs/types.h"

/* VIC-II layout when spy80 is active:
 *   Bank 3 ($C000-$FFFF) selected via vic_setmode().
 *   Screen RAM (colors): $C000-$C3E7  (1000 bytes, 40x25 cells)
 *   Bitmap:              $E000-$FF3F  (8000 bytes, 320x200 px)
 *   CPU writes bitmap by briefly setting $01=$34 (bank KERNAL out).
 *   vic_setmode(VICM_TEXT,0x0400,0x1000) restores character mode. */

#define SPY80_COLS   80
#define SPY80_ROWS   25
/* Row 24 is reserved for the sysop status line; caller content uses rows 0-23. */
#define SPY80_CONTENT_ROWS 24

/* Bitmap base address (under KERNAL ROM; VIC sees RAM, CPU banks out KERNAL) */
#define SPY80_BITMAP ((volatile u8 *)0xE000)

/* Screen RAM base (ordinary RAM at $C000) */
#define SPY80_SCRN   ((volatile u8 *)0xC000)

void spy80_init(void);
void spy80_done(void);
void spy80_feed(u8 ch);
void spy80_puts(u8 col, u8 row, const char *text, u8 vic_color);
void spy80_puts_rev(u8 col, u8 row, const char *text, u8 vic_color);  /* reverse bar */
void spy80_clear(void);
void spy80_scroll(void);   /* scroll content rows 0-23 up one; status row kept */

/* Internal ANSI parser entry points — called only from spy80.c */
void spy80_ansi_reset(void);
void spy80_ansi_feed(u8 ch);

#endif /* INCLUDE_BBS_SPY80_H */
