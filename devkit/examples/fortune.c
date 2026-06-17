/* fortune.c — example T/64 door: display a fortune and wait for a keypress.
 * Build: make door DOOR=fortune  (compiles this single file). */
#include "door_crt.h"

static const char *FORTUNES[] = {
    "YOU WILL FIX THE BUG.",
    "BEWARE THE OVERLAY ZONE.",
    "0 BYTES FREE AHEAD."
};

void door_main(void) {
    const bbs_api_t *b = bbs();
    bbs_caller_t me;
    u8 idx;
    if (!b) return;
    b->get_caller(&me);
    b->clear_screen();
    b->print("FORTUNE DOOR\r\n\r\nGREETINGS, ");
    b->print(me.handle);
    b->print("\r\n\r\n");
    /* Pick fortune by terminal width — avoids % which pulls in the divmod
     * runtime (sub-$9700 call); doors must make NO calls below $9700. */
    idx = me.term_width < 40 ? 0 : me.term_width < 80 ? 1 : 2;
    b->print(FORTUNES[idx]);
    b->print("\r\n\r\nPRESS A KEY...");
    b->getkey();
}
