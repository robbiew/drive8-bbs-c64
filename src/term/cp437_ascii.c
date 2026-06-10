/* CP437 -> 7-bit ASCII fallback.
 * Passthrough for 0x20-0x7E; CR/LF preserved; high bytes approximated.
 *
 * Ported from drive8-bbs src/term/cp437_ascii.c.
 */
#include "term_internal.h"

static const char cp437_to_ascii_high[128] = {
    /* 0x80-0x9F: accented chars — strip accent */
    'C','u','e','a','a','a','a','c','e','e','e','i','i','i','A','A',
    'E','a','A','o','o','o','u','u','y','O','U','c','L','Y','P','f',
    /* 0xA0-0xAF */
    'a','i','o','u','n','N','a','o','?','?','?','?','?','!','<','>',
    /* 0xB0-0xBF: shades and box — use ASCII approximations */
    '#','#','#','|','+','+','+','+','+','+','|','+','+','+','+','+',
    /* 0xC0-0xCF: box-drawing */
    '+','+','+','+','-','+','+','+','+','+','+','+','+','=','+','+',
    /* 0xD0-0xDF */
    '+','+','+','+','+','+','+','+','+','+','+','#','_','|','|','-',
    /* 0xE0-0xEF: greek and math */
    'a','B','G','p','S','s','u','t','F','T','O','d','o','f','e','^',
    /* 0xF0-0xFF */
    '=','+','>','<','/','/','/','~','*','.','.','v','n','2','#',' '
};

u8 term_xlate_byte_ascii(u8 cp437, u8 *out, u8 max)
{
    if (max == 0) return 0;
    if (cp437 == 0x0A || cp437 == 0x0D) {
        out[0] = cp437;
        return 1;
    }
    if (cp437 < 0x20) return 0;
    if (cp437 < 0x80) {
        out[0] = cp437;
        return 1;
    }
    out[0] = (u8)cp437_to_ascii_high[cp437 - 0x80];
    return 1;
}
