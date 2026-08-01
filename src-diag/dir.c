/* DIR - non-destructive directory listing of a device/partition. */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <c64/kernalio.h>
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"

int main(void)
{
    u8 dev = 10, part = 1;
    u16 n = 0;
    int c;
    char line[42];
    u8 col = 0;

    printf("\x93\x8e");
    printf("DIR OF DEV/PART\n");
    printf("DEVICE (8-11)? ");
    for (;;) { c = getch();
        if (c >= '8' && c <= '9') { dev = (u8)(c - '0'); break; }
        if (c == '1') { dev = 10; break; }
        if (c == '2') { dev = 11; break; } }
    printf("%u\nPARTITION (0-4)? ", (unsigned)dev);
    for (;;) { c = getch(); if (c >= '0' && c <= '4') { part = (u8)(c - '0'); break; } }
    printf("%u\n\n", (unsigned)part);

    if (disk_select_partition(dev, part) != BBS_OK) {
        printf("CP%u FAILED: %02u\n", (unsigned)part, disk_status(dev));
        printf("\nDONE.\n"); getch(); return 0;
    }

    krnio_setnam("$");
    if (!krnio_open(CFG_FNUM_DATA, dev, 0)) {
        printf("DIR OPEN FAILED\n"); printf("\nDONE.\n"); getch(); return 0;
    }
    line[0] = 0;
    while (n < 3000) {
        int v = krnio_getch(CFG_FNUM_DATA);
        char ch;
        if (v < 0) break;
        n++;
        ch = (char)(v & 0xFF);
        if (ch >= 0x20 && ch < 0x7F) {
            if (col < 39) line[col++] = ch;
        } else if (ch == 0 && col > 0) {
            line[col] = 0;
            if (col > 2) printf("%s\n", line);
            col = 0;
        }
    }
    krnio_clrchn();
    krnio_close(CFG_FNUM_DATA);
    printf("\n%u BYTES. DONE.\n", n);
    getch();
    return 0;
}
