/* PTEST - does partition selection really work, via the fixed HAL path? */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <c64/kernalio.h>

#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"

static u8 g_dev = 10;

/* Write NAME on partition p. Returns the DOS code (00 = written). */
static u8 wr(u8 p, const char *name)
{
    u8 code;
    disk_scratch(g_dev, p, name);
    (void)disk_status(g_dev);
    if (disk_open(g_dev, p, name, DISK_WRITE) != BBS_OK) return 99;
    disk_putline("PDATA");
    disk_close();
    code = disk_status(g_dev);
    return code;
}

/* Try to read NAME while partition p is selected. Returns the DOS code.
   62 = not found (partitions isolated). 00 = found. */
static u8 rd(u8 p, const char *name)
{
    if (disk_open(g_dev, p, name, DISK_READ) != BBS_OK) return 99;
    disk_close();
    return disk_status(g_dev);
}

int main(void)
{
    u8 a, b;

    printf("\x93\x8e");
    printf("T/64 PARTITION TEST\n\n");
    printf("DEVICE (8-11)? ");
    for (;;) {
        int c = getch();
        if (c >= '8' && c <= '9') { g_dev = (u8)(c - '0'); break; }
        if (c == '1') { g_dev = 10; break; }
        if (c == '2') { g_dev = 11; break; }
    }
    printf("%u\n", (unsigned)g_dev);

    (void)disk_status(g_dev);
    printf("ID: %s\n\n", disk_errmsg);

    printf("WRITE PT1 ON PART 1 : %02u\n", wr(1, "PT1"));
    printf("WRITE PT2 ON PART 2 : %02u\n", wr(2, "PT2"));
    printf("\nREAD BACK OWN FILE (want 00):\n");
    printf("  PART1 SEES PT1    : %02u\n", rd(1, "PT1"));
    printf("  PART2 SEES PT2    : %02u\n", rd(2, "PT2"));
    printf("\nCROSS (want 62 = ISOLATED):\n");
    a = rd(1, "PT2");
    b = rd(2, "PT1");
    printf("  PART1 SEES PT2    : %02u\n", a);
    printf("  PART2 SEES PT1    : %02u\n", b);
    printf("\n%s\n", (a == 62 && b == 62) ? "PARTITIONS ARE ISOLATED."
                                          : "SAME FILESYSTEM (1 PARTITION).");
    printf("\nDONE. PRESS A KEY.\n");
    getch();
    return 0;
}
