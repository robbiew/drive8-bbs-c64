/* WIPE - scratch every file on a device/partition. DESTRUCTIVE.
 * CBM DOS reports the number of files removed in the status line's
 * track field, e.g. "01,FILES SCRATCHED,32,00". */
#include <stdio.h>
#include <conio.h>
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"

static u8 g_dev = 10;

static void wipe(u8 part)
{
    bbs_err_t e;
    if (disk_select_partition(g_dev, part) != BBS_OK) {
        printf("P%u: CP FAILED %02u\n", (unsigned)part, disk_status(g_dev));
        return;
    }
    e = disk_cmd(g_dev, "S:*");
    printf("P%u: E%u %s\n", (unsigned)part, (unsigned)e, disk_errmsg);
}

int main(void)
{
    printf("\x93\x8e");
    printf("WIPE ALL FILES - DESTRUCTIVE\nDEV? ");
    for (;;) { int c = getch();
        if (c >= '8' && c <= '9') { g_dev = (u8)(c - '0'); break; }
        if (c == '1') { g_dev = 10; break; }
        if (c == '2') { g_dev = 11; break; } }
    printf("%u\n\nWIPE PARTITIONS 1 AND 2?\nPRESS Y TO CONFIRM\n", (unsigned)g_dev);
    for (;;) { int c = getch(); if (c=='Y'||c=='y') break; if (c=='N'||c=='n') { printf("\nCANCELLED.\n"); getch(); return 0; } }
    printf("\n");
    wipe(1);
    wipe(2);
    printf("\nDONE.\n");
    getch();
    return 0;
}
