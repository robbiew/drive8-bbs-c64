#include <stdio.h>
#include <string.h>
#include "bbs/callers.h"
#include "bbs/cfg.h"
#include "bbs/hal/clock.h"
#include "bbs/hal/disk.h"
#include "bbs/config.h"
#include "bbs/sysop.h"
#include "bbs/syscnt.h"

/* On-disk line formats:
 *  Old (35 chars): "MM/DD HH:MM A HHHHHHHHHHHHHH BBBBB"
 *  New (41 chars): "MM/DD HH:MM A HHHHHHHHHHHHHH BBBBB DDDDD"
 *  where DDDDD = session length in seconds (5 zero-padded digits).
 * CALLERS_FILE and CALLERS_LINE_MIN are in callers.h for shared access. */
#define CALLERS_LINE_FULL 41   /* new format with duration — always written */

/* ── helpers ─────────────────────────────────────────────────────── */

/* BCD byte → decimal u8 */
static u8 bcd_to_dec(u8 bcd)
{
    return (u8)(((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F));
}

/* Decimal u8 → BCD byte */
static u8 dec_to_bcd(u8 d)
{
    return (u8)(((d / 10) << 4) | (d % 10));
}

/* Write a 0-padded decimal field of `width` chars into buf (no NUL). */
static void fmt_dec(char *buf, u16 val, u8 width)
{
    u8 i = width;
    while (i--) {
        buf[i] = (char)('0' + (val % 10));
        val /= 10;
    }
}

/* ── callers_log ─────────────────────────────────────────────────── */

bbs_err_t callers_log(session_t *s, u16 elapsed_secs)
{
    char line[CALLERS_LINE_FULL + 2]; /* +1 NUL */
    clock_tod_t now;
    u8 hh, mm;
    char ap;
    const char *handle;
    u16 baud;
    u8 i;

    clock_read(&now);
    hh = bcd_to_dec(now.hours & 0x7F);
    mm = bcd_to_dec(now.mins);
    ap = (now.hours & 0x80) ? 'P' : 'A';

    handle = (s->handle[0] != '\0') ? s->handle : "[GUEST]";
    baud   = bbs_cfg.baud_rate;

    /* "MM/DD HH:MM A HHHHHHHHHHHHHH BBBBB DDDDD" */
    memcpy(line, wfc.date, 5);   /* MM/DD (first 5 chars of "MM/DD/YY") */
    line[5]  = ' ';
    fmt_dec(line + 6, hh, 2);
    line[8]  = ':';
    fmt_dec(line + 9, mm, 2);
    line[11] = ' ';
    line[12] = ap;
    line[13] = ' ';
    for (i = 0; i < 15 && handle[i] != '\0'; i++)
        line[14 + i] = handle[i];
    for (; i < 15; i++)
        line[14 + i] = ' ';
    line[29] = ' ';
    fmt_dec(line + 30, baud, 5);
    line[35] = ' ';
    fmt_dec(line + 36, elapsed_secs, 5);
    line[CALLERS_LINE_FULL] = '\0';

    if (cfg_send_drive_init(bbs_cfg.device_system, bbs_cfg.init_system) != BBS_OK)
        return BBS_EIO;
    if (disk_open(bbs_cfg.device_system, bbs_cfg.drive_system, CALLERS_FILE, DISK_APPEND) != BBS_OK)
        return BBS_EIO;
    disk_putline(line);
    disk_close();
    wfc.calls_today++;
    syscnt_save();
    return BBS_OK;
}

/* ── callers_load ────────────────────────────────────────────────── */

void callers_load(wfc_log_entry_t *buf, u8 max, u8 *got)
{
    char line[CALLERS_LINE_FULL + 2];  /* fits both old (35) and new (41) format */
    u8 head = 0;   /* circular write pos within buf */
    u8 count = 0;
    // cppcheck-suppress variableScope
    i16 len;
    wfc_log_entry_t *e;
    u8 hh, mm;
    u16 baud;
    u8 i;

    *got = 0;

    if (cfg_send_drive_init(bbs_cfg.device_system, bbs_cfg.init_system) != BBS_OK)
        return;

    if (disk_open(bbs_cfg.device_system, bbs_cfg.drive_system, CALLERS_FILE, DISK_READ) != BBS_OK)
        return;  /* no file yet — silently skip */

    while (!disk_eof()) {
        len = disk_gets(line, (u8)(CALLERS_LINE_FULL + 1));
        if (len < CALLERS_LINE_MIN) continue;  /* skip short/garbled lines */

        /* Parse into circular buf */
        e = &buf[head];

        /* Time: chars 6-7 HH, 9-10 MM, 12 A/P */
        hh = (u8)((line[6] - '0') * 10 + (line[7] - '0'));
        mm = (u8)((line[9] - '0') * 10 + (line[10] - '0'));
        e->time.hours  = dec_to_bcd(hh);
        if (line[12] == 'P') e->time.hours |= 0x80;
        e->time.mins   = dec_to_bcd(mm);
        e->time.secs   = 0;
        e->time.tenths = 0;

        /* Handle: chars 14-28 (15 chars, space-padded in file) */
        for (i = 0; i < 15; i++) e->handle[i] = line[14 + i];
        for (i = 14; i > 0 && e->handle[i] == ' '; i--) e->handle[i] = '\0';
        e->handle[15] = '\0';

        /* Baud: chars 30-34 */
        baud = 0;
        for (i = 30; i <= 34; i++) baud = (u16)(baud * 10 + (line[i] - '0'));
        e->baud = baud;

        /* Duration (secs): chars 36-40 — only in new (41-char) format */
        if (len >= CALLERS_LINE_FULL) {
            u16 dur = 0;
            for (i = 36; i <= 40; i++) dur = (u16)(dur * 10 + (line[i] - '0'));
            e->duration = dur;
        } else {
            e->duration = 0;
        }

        head = (u8)((head + 1) % max);
        if (count < max) count++;
    }

    disk_close();

    /* Rotate buf so oldest entry is at index 0, newest at count-1.
     * After the loop, `head` points to the slot AFTER the last write.
     * If buffer wrapped, oldest is at `head`; if not, oldest is at 0. */
    if (count == max) {
        /* Buffer wrapped: entries are in order starting from `head`.
         * Rotate in-place using a temp copy on stack (max <= 7 entries). */
        wfc_log_entry_t tmp[7];
        u8 j;
        for (j = 0; j < max; j++)
            tmp[j] = buf[(head + j) % max];
        for (j = 0; j < max; j++)
            buf[j] = tmp[j];
    }
    /* If count < max, entries already sit at buf[0..count-1] in order. */

    *got = count;
}
