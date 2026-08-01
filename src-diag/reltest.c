/* RELTEST - can this device CREATE and use a REL file?
 *
 * WHY no disk_status() between rel_open and rel_close: disk_status() closes
 * and reopens logical file 15, which is the command channel rel_position()
 * drives the seek through. Calling it mid-sequence breaks positioning on a
 * perfectly good drive - it made an earlier version of this test report
 * "REL BROKEN" against a 1581 that works fine. Collect results, print after. */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"
#include "bbs/rel.h"

int main(void)
{
    u8 dev = 10;
    rel_handle_t h;
    bbs_err_t eo, ep, ew, er;
    u8 buf[23], got = 0, first = 0;

    printf("\x93\x8e");
    printf("REL TEST\nDEV? ");
    for (;;) { int c = getch();
        if (c >= '8' && c <= '9') { dev = (u8)(c - '0'); break; }
        if (c == '1') { dev = 10; break; }
        if (c == '2') { dev = 11; break; } }
    printf("%u\n", (unsigned)dev);

    disk_scratch(dev, 0, "RELTST");

    rel_reset();
    eo = rel_open(dev, 0, "RELTST", 23, &h);
    ep = ew = er = BBS_EFATAL;
    if (eo == BBS_OK) {
        memset(buf, ' ', sizeof(buf));
        buf[0] = 1;
        ep = rel_position(h, 1);
        ew = rel_write(h, buf, 23);
        memset(buf, 0, sizeof(buf));
        rel_position(h, 1);
        er = rel_read(h, buf, 23, &got);
        first = buf[0];
        rel_close(h);
    }

    printf("OPEN  E%u\n", (unsigned)eo);
    printf("POS   E%u\n", (unsigned)ep);
    printf("WRITE E%u\n", (unsigned)ew);
    printf("READ  E%u g%u b%u\n", (unsigned)er, (unsigned)got, (unsigned)first);
    printf("\n%s\n", (eo == BBS_OK && er == BBS_OK && got == 23 && first == 1)
                     ? "REL WORKS" : "REL FAILS");
    printf("DONE.\n");
    getch();
    return 0;
}
