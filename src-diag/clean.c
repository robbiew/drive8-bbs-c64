/* CLEAN - scratch T/64's system files from one device/partition.
 * For removing orphans left by a re-pointed config. Message-base files
 * (BOARDS, USR.PTR, B<n>.IDX) are deliberately NOT touched. */
#include <stdio.h>
#include <conio.h>
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"

static u8 g_dev = 10;

static void kill_one(u8 part, const char *name)
{
    u8 code;
    disk_scratch(g_dev, part, name);
    code = disk_status(g_dev);
    printf("%-9s %02u\n", name, code);
}

int main(void)
{
    u8 part = 1;
    printf("\x93\x8e");
    printf("CLEAN T/64 SYSTEM FILES\nDEV? ");
    for (;;) { int c = getch();
        if (c >= '8' && c <= '9') { g_dev = (u8)(c - '0'); break; }
        if (c == '1') { g_dev = 10; break; }
        if (c == '2') { g_dev = 11; break; } }
    printf("%u\nPART? ", (unsigned)g_dev);
    for (;;) { int c = getch(); if (c >= '0' && c <= '4') { part = (u8)(c - '0'); break; } }
    printf("%u\n\n", (unsigned)part);

    kill_one(part, "USR LOG");
    kill_one(part, "USR PROF");
    kill_one(part, "ACCESS");
    kill_one(part, "CALLERS");
    kill_one(part, "STATUS");
    kill_one(part, "SYSCNT");
    kill_one(part, "USR.DAY");
    printf("\nDONE.\n");
    getch();
    return 0;
}
