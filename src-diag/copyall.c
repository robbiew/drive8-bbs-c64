/* COPYALL - copy the T/64 disk set from one device to another device/partition.
 *
 * Uses two distinct KERNAL logical file numbers so source and destination are
 * open at once - T/64's disk HAL keeps a single data channel, so it cannot do
 * this itself. Files are copied through the C64, which is the point: sd2iec
 * then generates the FAT name, so names come out as proper CBM names instead
 * of carrying a ".PRG" the way a PC-side copy does. */
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <c64/kernalio.h>
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/hal/disk.h"
#include "bbs/version.h"

#define LF_SRC 2
#define LF_DST 3

/* Each entry carries its own PRG/SEQ flag instead of a parallel "first N are
   PRG" count - adding or reordering an entry can't desync a hand-maintained
   number from the array again. BOOT/CONFIGURE names are built from
   BBS_RELEASE_VERSION_COMPACT (C99 adjacent string-literal concatenation) so
   this list can never drift from the version the Makefile actually names the
   PRGs after.

   Program and content files only. CONFIG, CALLERS and ACCESS are deliberately
   NOT copied: they are per-install data, and overwriting a SysOp's device
   configuration during a program update is destructive. */
typedef struct {
    const char *name;
    bool_t is_prg;
} copy_file_t;

static const copy_file_t files[] = {
    { "BOOT-" BBS_RELEASE_VERSION_COMPACT,      TRUE },
    { "CONFIGURE-" BBS_RELEASE_VERSION_COMPACT, TRUE },
    { "OVL_MSGS",    TRUE },
    { "OVL_WFC",     TRUE },
    { "OVL_BOOT",    TRUE },
    { "OVL_DOORS",   TRUE },
    { "OVL_FILES",   TRUE },
    { "OVL_ZMODEM",  TRUE },
    { "OVL_AUTH",    TRUE },
    { "FORTUNE",     TRUE },
    { "G.LOGIN",      FALSE }, { "G.LOGIN 0",   FALSE },
    { "G.LOGIN 1 80", FALSE }, { "G.LOGIN 2 80", FALSE },
    { "G.NEWUSER",    FALSE }, { "G.TERM",       FALSE },
    { "M.DOORS",      FALSE }, { "M.FILES",      FALSE },
    { "M.MAIN",       FALSE }, { "M.MAIN 1 80",  FALSE },
    { "M.MSGS",       FALSE }, { "M.MSGS 1 80",  FALSE },
    { "M.READ",       FALSE },
    { "P.DOORS",      FALSE }, { "P.FILES",      FALSE },
    { "P.MAIN",       FALSE }, { "P.MAIN 1 80",  FALSE },
    { "P.MSGS",       FALSE }, { "P.READ",       FALSE },
    { "P.READ 1 80",  FALSE }
};
#define NFILE (sizeof(files)/sizeof(files[0]))

static u8 buf[128];

/* Per-chunk source read status (krnio_pstatus[LF_SRC], captured right after
   krnio_read). Held file-static, not a copy_one() local: it must stay valid
   across the krnio_write()/krnio_status() calls later in the same loop
   iteration, and oscar64 gives functions static per-function frames that can
   overlap with a callee's frame across a call (see src/data/users.c:25-30).
   A stack local surviving a call is exactly the shape of that hazard; a
   fixed BSS slot is immune. */
static krnioerr s_read_status;

static bool_t copy_one(u8 sdev, u8 ddev, const char *name, bool_t is_prg)
{
    char sn[40], dn[40];
    bool_t ok = FALSE;

    sprintf(sn, "0:%s,%c,R", name, is_prg ? 'P' : 'S');
    sprintf(dn, "0:%s,%c,W", name, is_prg ? 'P' : 'S');

    krnio_setnam(dn + 2);            /* scratch any old copy first */
    disk_scratch(ddev, 1, name);
    (void)disk_status(ddev);

    krnio_setnam(sn);
    if (!krnio_open(LF_SRC, sdev, LF_SRC)) return FALSE;
    krnio_setnam(dn);
    if (!krnio_open(LF_DST, ddev, LF_DST)) { krnio_close(LF_SRC); return FALSE; }

    /* Loop until krnio_read reports clean end-of-file. Do NOT treat every
       n <= 0 as end-of-file: krnio_read() returns -1 only when CHKIN on the
       source channel fails outright, but a mid-stream drive error (timeout,
       checksum, device dropped) can also come back as a SHORT read - even
       n == 0 - without going negative. The only reliable signal is the
       per-channel status krnio_read() itself latches into krnio_pstatus[]
       (see vendor/oscar64/include/c64/kernalio.c): KRNIO_OK/KRNIO_EOF mean
       the bytes actually in `buf` are good, anything else is a real error.
       Reading krnio_pstatus[LF_SRC] directly (not krnio_status()) matters
       because krnio_status() reflects the KERNAL's single shared ST
       register, which the krnio_write() call below clobbers with the
       destination channel's status - see disk_write()'s comment in
       src/hal/disk.c for the same caveat on the write side.

       A truncated PRG still LOADs successfully and then crashes on RUN,
       which is a miserable thing to debug - so on any doubt we fail the
       file rather than print a false '.'. */
    for (;;) {
        int n = krnio_read(LF_SRC, (char *)buf, (int)sizeof(buf));
        if (n < 0) break;                      /* source CHKIN failed outright */

        s_read_status = krnio_pstatus[LF_SRC];
        if (s_read_status != KRNIO_OK && s_read_status != KRNIO_EOF) break; /* mid-stream read error */

        if (n == 0) { ok = TRUE; break; }        /* clean EOF, nothing left to flush */

        if (krnio_write(LF_DST, (const char *)buf, n) != n) break;
        /* Mirrors disk_write(): krnio_write() returns `n` once CHKOUT
           succeeds, before the bytes are confirmed on the drive. */
        if (krnio_status() != KRNIO_OK) break;

        if (s_read_status == KRNIO_EOF) { ok = TRUE; break; }
    }

    krnio_clrchn();
    krnio_close(LF_SRC);
    krnio_close(LF_DST);
    return ok;
}

int main(void)
{
    u8 sdev = 8, ddev = 10, i, bad = 0;

    printf("\x93\x8e");
    printf("COPY T/64 SET  8 -> 10 PART 1\n");
    printf("PRESS C TO START\n");
    for (;;) { int c = getch(); if (c == 'C' || c == 'c') break; }
    printf("\n");

    if (disk_select_partition(ddev, 1) != BBS_OK) {
        printf("CP1 FAILED %02u\n", disk_status(ddev));
        getch(); return 0;
    }

    for (i = 0; i < (u8)NFILE; i++) {
        bool_t ok = copy_one(sdev, ddev, files[i].name, files[i].is_prg);
        printf("%c %s\n", ok ? '.' : '!', files[i].name);
        if (!ok) bad++;
    }
    printf("\nDONE. %u FAILED.\n", (unsigned)bad);
    getch();
    return 0;
}
