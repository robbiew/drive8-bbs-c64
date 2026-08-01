/* CPTEST - what status code does CP<n> actually return per device? */
#include <stdio.h>
#include <conio.h>
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"

int main(void)
{
    u8 dev = 8, p;
    printf("\x93\x8e");
    printf("CP TEST\nDEV? ");
    for (;;) { int c = getch();
        if (c >= '8' && c <= '9') { dev = (u8)(c - '0'); break; }
        if (c == '1') { dev = 10; break; }
        if (c == '2') { dev = 11; break; } }
    printf("%u\n", (unsigned)dev);
    for (p = 0; p <= 3; p++) {
        char cmd[8];
        bbs_err_t e;
        sprintf(cmd, "CP%u", (unsigned)p);
        e = disk_cmd(dev, cmd);
        printf("CP%u E%u D%02u\n", (unsigned)p, (unsigned)e, disk_status(dev));
    }
    printf("\nDONE.\n");
    getch();
    return 0;
}
