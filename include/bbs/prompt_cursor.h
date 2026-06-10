/* bbs/prompt_cursor.h - animated color-cycling block cursor at the menu prompt. */
#ifndef BBS_PROMPT_CURSOR_H
#define BBS_PROMPT_CURSOR_H

#include "bbs/types.h"
#include "bbs/term.h"
#include "bbs/session.h"

#define PROMPT_CURSOR_PALETTE_LEN 5

/* Build the byte sequence that draws one block frame in `color_idx` (taken
 * modulo the palette length) for `mode`, writing a NUL-terminated string into
 * `out` (>= 10 bytes; callers use 16) and returning its length. Returns 0
 * (empty `out`) for modes without color (TERM_ASCII and any other).
 * Pure: no I/O, no globals. */
u8 prompt_cursor_build_frame(term_mode_t mode, u8 color_idx, char *out);

/* I/O wiring, driven from the SESS_IN_MENU poll.
 * Caller must invoke prompt_cursor_clear() to reset terminal color state when
 * the animation ends. */
void prompt_cursor_arm(const session_t *s);
void prompt_cursor_tick(const session_t *s);
void prompt_cursor_clear(const session_t *s);

#endif /* BBS_PROMPT_CURSOR_H */
