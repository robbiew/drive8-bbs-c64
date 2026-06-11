/* Host round-trip tests for term_unxlate_byte (inbound wire -> CP437). */
#include "host.h"
#include "bbs/term.h"

int main(void) {
    static const term_mode_t modes[] =
        { TERM_PETSCII, TERM_ANSI_CP437, TERM_ASCII, TERM_PETSCII_LOWER };
    unsigned m, b;

    /* Round-trip over the typeable ASCII range: unxlate(xlate(b)) == b,
     * but only where the forward map is a single byte (dropped/multi-byte
     * forwards are not round-trippable and are out of scope for input). */
    for (m = 0; m < 4; m++) {
        for (b = 0x20; b <= 0x7E; b++) {
            u8 out[4];
            u8 n = term_xlate_byte(modes[m], (u8)b, out, (u8)sizeof(out));
            if (n != 1) continue;
            EXPECT_EQ("roundtrip", term_unxlate_byte(modes[m], out[0]), (long)b);
        }
    }

    /* PETSCII text-mode letter swap, explicit both directions. */
    EXPECT_EQ("pl.lower_a", term_unxlate_byte(TERM_PETSCII_LOWER, 0x41), 0x61);
    EXPECT_EQ("pl.upper_A", term_unxlate_byte(TERM_PETSCII_LOWER, 0xC1), 0x41);
    EXPECT_EQ("pl.digit",   term_unxlate_byte(TERM_PETSCII_LOWER, 0x31), 0x31);

    /* Control bytes pass through unchanged (so CR/backspace/DEL survive). */
    EXPECT_EQ("ctl.cr",  term_unxlate_byte(TERM_PETSCII_LOWER, 0x0D), 0x0D);
    EXPECT_EQ("ctl.bs",  term_unxlate_byte(TERM_PETSCII_LOWER, 0x08), 0x08);
    EXPECT_EQ("ctl.del", term_unxlate_byte(TERM_PETSCII_LOWER, 0x14), 0x14);

    /* PETSCII uppercase/graphics, ANSI, and ASCII are identity for printable. */
    EXPECT_EQ("petscii.id", term_unxlate_byte(TERM_PETSCII, 0x41), 0x41);
    EXPECT_EQ("ansi.id",    term_unxlate_byte(TERM_ANSI_CP437, 0x41), 0x41);
    EXPECT_EQ("ascii.id",   term_unxlate_byte(TERM_ASCII, 0x61), 0x61);

    return test_summary("term_unxlate");
}
