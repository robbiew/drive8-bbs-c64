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
