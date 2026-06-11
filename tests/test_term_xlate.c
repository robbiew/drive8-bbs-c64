/* Characterization dump: every CP437 byte through every wire mode.
 * tools/test.sh diffs the output against tests/golden/term_xlate.txt;
 * a table change is only legal with a reviewed golden update. */
#include <stdio.h>
#include "bbs/term.h"

int main(void) {
    static const term_mode_t modes[] =
        { TERM_PETSCII, TERM_ANSI_CP437, TERM_ASCII, TERM_PETSCII_LOWER };
    unsigned m, b;
    u8 i;
    for (m = 0; m < 4; m++) {
        for (b = 0; b < 256; b++) {
            u8 out[4];
            u8 n = term_xlate_byte(modes[m], (u8)b, out, (u8)sizeof(out));
            printf("%u %02x:", m, b);
            for (i = 0; i < n; i++) printf(" %02x", out[i]);
            printf("\n");
        }
    }
    return 0;
}
