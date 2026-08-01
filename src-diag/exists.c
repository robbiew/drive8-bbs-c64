/* EXISTS - is a given file present on a device/partition?
 * Uses the DOS status code, not disk_open()'s return: KERNAL OPEN succeeds on
 * any device that answers, so only the status code is evidence (62 = absent). */
#include <stdio.h>
#include <conio.h>
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"

static u8 g_dev = 10;

static void chk(u8 part, const char *name)
{
    u8 code;
    if (disk_open(g_dev, part, name, DISK_READ) == BBS_OK) disk_close();
    code = disk_status(g_dev);
    printf("P%u %-8s %02u %s\n", (unsigned)part, name, code,
           code == 62 ? "ABSENT" : (code == 64 ? "PRESENT" : (code < 20 ? "PRESENT" : "")));
}

int main(void)
{
    printf("\x93\x8e");
    printf("FILE CHECK\nDEV? ");
    for (;;) { int c = getch();
        if (c >= '8' && c <= '9') { g_dev = (u8)(c - '0'); break; }
        if (c == '1') { g_dev = 10; break; }
        if (c == '2') { g_dev = 11; break; } }
    printf("%u\n", (unsigned)g_dev);
    chk(1, "USR LOG"); chk(1, "BOARDS");
    chk(2, "USR LOG"); chk(2, "BOARDS");
    printf("\nDONE.\n");
    getch();
    return 0;
}
