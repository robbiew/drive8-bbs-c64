#ifndef INCLUDE_BBS_SSTATUS_H
#define INCLUDE_BBS_SSTATUS_H

#include "types.h"
#include "err.h"

/* Sysop "SYSOP IS" status line persistence.
 * Stored in a tiny STATUS file on the system device, separate from CONFIG,
 * so a bad write can never corrupt device/identity settings. Shared by
 * BOOT (F7 SET STATUS) and CONFIGURE (config editor field). */

/* Load persisted status line into buf (buf[0]='\0' if file missing/empty).
 * bufsize includes the NUL; trailing CR/LF is stripped. */
void sstatus_load(char *buf, u8 bufsize);

/* Persist status line (<= 20 chars) to the STATUS file. */
bbs_err_t sstatus_save(const char *msg);

#endif /* INCLUDE_BBS_SSTATUS_H */
