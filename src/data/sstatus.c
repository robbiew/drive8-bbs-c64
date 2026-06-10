/**
 * TURBO/64 BBS — Sysop status line persistence
 *
 * Reads/writes the WFC "SYSOP IS" status line to a tiny STATUS file so it
 * survives reboots and is shared between BOOT and CONFIGURE. Kept separate
 * from CONFIG so a status write can never corrupt device/identity settings.
 */

#include "bbs/sstatus.h"
#include "bbs/cfg.h"
#include "bbs/hal/disk.h"

#define SSTATUS_FILE "STATUS"

void sstatus_load(char *buf, u8 bufsize)
{
    i16 n;
    buf[0] = '\0';
    if (cfg_send_drive_init(bbs_cfg.device_system, bbs_cfg.init_system) != BBS_OK)
        return;
    if (disk_open(bbs_cfg.device_system, bbs_cfg.drive_system,
                  SSTATUS_FILE, DISK_READ) != BBS_OK)
        return;
    n = disk_gets(buf, bufsize);
    disk_close();
    if (n <= 0) { buf[0] = '\0'; return; }
    /* krnio_gets includes the trailing CR/LF terminator — strip it. */
    while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n'))
        buf[--n] = '\0';
}

bbs_err_t sstatus_save(const char *msg)
{
    if (cfg_send_drive_init(bbs_cfg.device_system, bbs_cfg.init_system) != BBS_OK)
        return BBS_EIO;
    disk_scratch(bbs_cfg.device_system, bbs_cfg.drive_system, SSTATUS_FILE);
    if (disk_open(bbs_cfg.device_system, bbs_cfg.drive_system,
                  SSTATUS_FILE, DISK_WRITE) != BBS_OK)
        return BBS_EIO;
    disk_putline(msg);
    disk_close();
    return BBS_OK;
}
