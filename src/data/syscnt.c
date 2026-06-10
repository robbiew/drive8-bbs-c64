/**
 * TURBO/64 BBS — System Counters Persistence
 *
 * Reads/writes calls_today and posts_today to a tiny SYSCNT file, stamped with
 * the date so the counters auto-reset on the first boot of a new day.
 * Format: one CR-terminated line "MM/DD/YY CCCCC PPPPP" (zero-padded decimals).
 * Minimal code size — runs on every session end and BBS boot.
 */

#include "bbs/syscnt.h"
#include "bbs/sysop.h"
#include "bbs/cfg.h"
#include "bbs/hal/disk.h"
#include <stdio.h>
#include <string.h>

#define SYSCNT_FILE "SYSCNT"
#define SYSCNT_DATELEN 8           /* "MM/DD/YY" */

/* Parse up to 5 decimal ASCII digits from s into a u16. */
static u16 syscnt_parse(const char *s)
{
    u16 v = 0;
    while (*s >= '0' && *s <= '9')
        v = (u16)(v * 10u + (u16)(*s++ - '0'));
    return v;
}

void syscnt_load(void)
{
    bool_t same_day = FALSE;

    if (cfg_send_drive_init(bbs_cfg.device_system, bbs_cfg.init_system) == BBS_OK &&
        disk_open(bbs_cfg.device_system, bbs_cfg.drive_system,
                  SYSCNT_FILE, DISK_READ) == BBS_OK) {
        char buf[24];
        /* "MM/DD/YY CCCCC PPPPP" = 20 chars; require the date to match today. */
        if (disk_gets(buf, (u8)sizeof(buf)) >= 20 &&
            strncmp(buf, wfc.date, SYSCNT_DATELEN) == 0) {
            wfc.calls_today = syscnt_parse(buf + 9);
            wfc.posts_today = syscnt_parse(buf + 15);
            same_day = TRUE;
        }
        disk_close();
    }

    if (!same_day) {
        /* New day, missing file, or legacy format → reset and stamp today. */
        wfc.calls_today = 0;
        wfc.posts_today = 0;
        syscnt_save();
    }
}

bbs_err_t syscnt_save(void)
{
    char buf[24];
    if (cfg_send_drive_init(bbs_cfg.device_system, bbs_cfg.init_system) != BBS_OK)
        return BBS_EIO;
    disk_scratch(bbs_cfg.device_system, bbs_cfg.drive_system, SYSCNT_FILE);
    if (disk_open(bbs_cfg.device_system, bbs_cfg.drive_system,
                  SYSCNT_FILE, DISK_WRITE) != BBS_OK)
        return BBS_EIO;
    sprintf(buf, "%-8s %05u %05u", wfc.date,
            (unsigned)wfc.calls_today, (unsigned)wfc.posts_today);
    disk_putline(buf);
    disk_close();
    return BBS_OK;
}

bbs_err_t syscnt_init(u8 device)
{
    char buf[24];
    if (cfg_send_drive_init(device, bbs_cfg.init_system) != BBS_OK)
        return BBS_EIO;
    disk_scratch(device, bbs_cfg.drive_system, SYSCNT_FILE);
    if (disk_open(device, bbs_cfg.drive_system, SYSCNT_FILE, DISK_WRITE) != BBS_OK)
        return BBS_EIO;
    /* Sentinel date 00/00/00 never matches a real boot date, so the first
     * boot resets the counters and stamps the correct day. wfc-free so the
     * editor (which has no wfc global) can call it from INIT. */
    sprintf(buf, "%-8s %05u %05u", "00/00/00", 0u, 0u);
    disk_putline(buf);
    disk_close();
    return BBS_OK;
}
