/* bbs/term.h - terminal output mode and byte-level translation.
 *
 * All BBS content is authored and stored in CP437 encoding.
 * term_xlate_byte() translates a single CP437 byte to the wire
 * encoding appropriate for the caller's terminal.
 *
 * PETSCII callers:  native C64 + U64 local console.
 * ANSI/CP437:       SyncTerm, NetRunner, telnet clients with CP437.
 * ASCII:            plain 7-bit fallback, high bytes stripped/approx.
 */
#ifndef BBS_TERM_H
#define BBS_TERM_H

#include "bbs/types.h"

typedef enum {
    TERM_PETSCII       = 0,
    TERM_ANSI_CP437    = 1,
    TERM_ASCII         = 2,
    TERM_PETSCII_LOWER = 3   /* internal dispatch token; never stored in user_record */
} term_mode_t;

/* Translate one CP437 byte to the given mode, writing up to `max`
 * bytes into `out`.  Returns the number of bytes written (0-2).
 * Multi-byte output only occurs for PETSCII escape sequences. */
u8 term_xlate_byte(term_mode_t mode, u8 cp437, u8 *out, u8 max);

#endif /* BBS_TERM_H */
