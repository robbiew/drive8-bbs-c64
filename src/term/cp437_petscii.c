/* CP437 -> PETSCII byte translation.
 *
 * Content is authored in CP437.  This table maps each CP437 byte to its
 * PETSCII wire value.  0x00 in the table means "drop this byte" (no
 * PETSCII equivalent).
 *
 * Ported from drive8-bbs src/term/cp437_petscii.c.
 */
#include "term_internal.h"

static const u8 cp437_to_petscii[256] = {
    /* 0x00-0x0F */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   9,  13,   0,   0,  13,   0,   0,
    /* 0x10-0x1F */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    /* 0x20-0x2F */
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    /* 0x30-0x3F */
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    /* 0x40-0x4F */
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    /* 0x50-0x5F */
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
    /* 0x60-0x6F — lowercase; session start sends CHR$(14) for lowercase mode */
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    /* 0x70-0x7F */
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x3F,

    /* 0x80-0xAF: accented Latin — no PETSCII coverage */
    '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',
    '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',
    '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',

    /* 0xB0-0xBF: shades and box-drawing */
    0xA6,    /* B0 light shade */
    0xA6,    /* B1 medium shade */
    0xE6,    /* B2 dark shade */
    0xDD,    /* B3 vertical bar */
    '?','?','?','?',
    '?','?','?','?',
    0xAE,    /* BC bottom-right corner */
    '?',
    0xBE,    /* BE top-right corner approx */
    0xAE,    /* BF top-right corner */

    /* 0xC0-0xCF: box-drawing */
    0xAD,    /* C0 bottom-left */
    0xC0,    /* C1 bottom-T */
    0xC0,    /* C2 top-T */
    0xDD,    /* C3 left-T */
    0xC0,    /* C4 horizontal */
    0x2B,    /* C5 cross */
    '?','?','?','?','?','?','?','?',
    0xB0,    /* CE top-left (single approx) */
    '?',

    /* 0xD0-0xDF */
    '?','?','?','?','?','?','?','?','?','?','?','?',
    0xA0,    /* DC lower half block -> reverse space */
    '?','?',
    0xA0,    /* DF upper half block */

    /* 0xE0-0xFF */
    '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',
    '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?'
};

u8 term_xlate_byte_petscii(u8 cp437, u8 *out, u8 max)
{
    if (max == 0) return 0;
    u8 v = cp437_to_petscii[cp437];
    if (v == 0) return 0;
    out[0] = v;
    return 1;
}

/* Lowercase/text-charset variant: the caller's terminal is in text charset
 * (CHR$14).  Remap only the letters so they render with correct case there:
 *   CP437 A-Z (0x41-0x5A) -> PETSCII 0xC1-0xDA  (uppercase in text charset)
 *   CP437 a-z (0x61-0x7A) -> PETSCII 0x41-0x5A  (lowercase in text charset)
 * Everything else (digits, punctuation, shades, box-drawing) is identical to
 * the uppercase table, so delegate it. */
u8 term_xlate_byte_petscii_lower(u8 cp437, u8 *out, u8 max)
{
    if (max == 0) return 0;
    if (cp437 >= 0x41u && cp437 <= 0x5Au) { out[0] = (u8)(cp437 + 0x80u); return 1; }
    if (cp437 >= 0x61u && cp437 <= 0x7Au) { out[0] = (u8)(cp437 - 0x20u); return 1; }
    return term_xlate_byte_petscii(cp437, out, max);
}
