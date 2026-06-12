/* Top-level term dispatcher. */
#include "bbs/term.h"
#include "term_internal.h"

u8 term_xlate_byte(term_mode_t mode, u8 cp437, u8 *out, u8 max)
{
    switch (mode) {
    case TERM_PETSCII:
        return term_xlate_byte_petscii(cp437, out, max);
    case TERM_PETSCII_LOWER:
        return term_xlate_byte_petscii_lower(cp437, out, max);
    case TERM_ANSI_CP437:
        if (max < 1) return 0;
        out[0] = cp437;
        return 1;
    case TERM_ASCII:
        return term_xlate_byte_ascii(cp437, out, max);
    }
    /* Unknown/legacy mode: pass the CP437 byte through unchanged. */
    if (max < 1) return 0;
    out[0] = cp437;
    return 1;
}

u8 term_unxlate_byte(term_mode_t mode, u8 wire)
{
    switch (mode) {
    case TERM_PETSCII_LOWER:
        /* Text/lowercase charset.  In this charset 0x41-0x5A render as
         * lowercase and BOTH 0x61-0x7A and 0xC1-0xDA render as uppercase, and
         * terminals send the matching code: unshifted letters arrive as
         * 0x41-0x5A (lowercase), Shift-ed letters as 0x61-0x7A (SyncTerm) or
         * 0xC1-0xDA.  Map all three ranges back to CP437. */
        if (wire >= 0x41u && wire <= 0x5Au) return (u8)(wire + 0x20u); /* lower */
        if (wire >= 0x61u && wire <= 0x7Au) return (u8)(wire - 0x20u); /* UPPER */
        if (wire >= 0xC1u && wire <= 0xDAu) return (u8)(wire - 0x80u); /* UPPER */
        return wire;
    case TERM_PETSCII:
    case TERM_ANSI_CP437:
    case TERM_ASCII:
        break;
    }
    /* PETSCII uppercase/graphics, ANSI/CP437, ASCII, and unknown modes:
     * identity over the typeable range (uppercase PETSCII letters already
     * equal CP437). Mirrors term_xlate_byte's switch so -Wswitch flags any
     * future mode that needs a non-identity inverse. */
    return wire;
}
