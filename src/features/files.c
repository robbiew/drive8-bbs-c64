/* files.c — File area browse, Punter download/upload.  OVL_FILES section.
 *
 * Entry: files_run(s) — called by action_files() (RESIDENT) after loading
 * OVL_FILES.  Mirrors bulletin.c pattern: m.files header, p.files prompt,
 * MCI area name, +/- area navigation, digit-select, L/D/U/A/Q commands.
 * Zmodem: via RESIDENT xfer_zmodem_send/recv (stub until implemented).
 * Punter send/receive are compiled into this overlay (files_code section). */
#include "bbs/overlay.h"
#include "bbs/session.h"
#include "bbs/files.h"
#include "bbs/file_areas.h"
#include "bbs/file_entries.h"
#include "bbs/xfer.h"
#include "bbs/cfg.h"
#include "bbs/records.h"
#include "bbs/config.h"
#include "net/punter.h"
#include <string.h>
#include <stdio.h>

#pragma code(files_code)
#pragma data(files_data)
#pragma bss(files_bss)

/* ── Output helpers ─────────────────────────────────────────────────────── */

static void ftx(session_t *s, const char *t)  { session_emit(s, t); }
static void fnl(session_t *s)                  { session_emit(s, "\r\n"); }
static void fl(session_t *s, const char *t)    { ftx(s, t); fnl(s); }

/* ── Input: hotkey + optional digit collection (like bull_getkey) ────────── */

static void fgetkey_cmd(session_t *s, char *cmd)
{
    u8 ch; char e[2]; u8 len;
    cmd[0] = '\0';
    while (!sess_read_key(s, &ch))
        if (!sess_carrier_ok(s)) return;
    if (ch == 10 || ch == 13) { fnl(s); return; }
    if (ch >= 'a' && ch <= 'z') ch = (u8)(ch - 32);
    e[0] = (char)ch; e[1] = '\0';
    ftx(s, e);
    cmd[0] = (char)ch; cmd[1] = '\0';
    /* Digits: collect until ENTER (for area/file number) */
    if (ch >= '0' && ch <= '9') {
        len = 1;
        while (len < 4) {
            while (!sess_read_key(s, &ch))
                if (!sess_carrier_ok(s)) { fnl(s); return; }
            if (ch == 13 || ch == 10) break;
            if (ch >= '0' && ch <= '9') {
                e[0] = (char)ch; ftx(s, e);
                cmd[len++] = (char)ch; cmd[len] = '\0';
            }
        }
    }
    fnl(s);
}

/* ── Area management ────────────────────────────────────────────────────── */

/* Load area by sequential index (1-based). Returns 1 on success. */
static u8 fl_load_area(session_t *s, u8 idx, ud_area_record_t *out)
{
    if (file_area_by_index(idx, out, bbs_cfg.device_files) != BBS_OK) return 0;
    if (out->access_level > s->user.access_level) return 0;
    return 1;
}

/* Advance to next (+) or previous (-) accessible area. */
static void fl_shift_area(session_t *s, u8 fwd, u8 total,
                           ud_area_record_t *area, u8 *pidx)
{
    u8 idx = *pidx, tries;
    ud_area_record_t rec;
    for (tries = 0; tries < total; tries++) {
        if (fwd) idx = (idx >= total) ? 1 : (u8)(idx + 1);
        else     idx = (idx <= 1)     ? total : (u8)(idx - 1);
        if (file_area_by_index(idx, &rec, bbs_cfg.device_files) == BBS_OK &&
            rec.access_level <= s->user.access_level) {
            *area = rec;
            *pidx = idx;
            return;
        }
    }
    fl(s, "NO ACCESSIBLE AREAS.");
}

/* ── Area listing ───────────────────────────────────────────────────────── */

static void fl_list_areas(session_t *s)
{
    ud_area_record_t rec;
    u8 i, total, found = 0;
    char buf[44];
    total = file_area_count(bbs_cfg.device_files);
    for (i = 1; i <= total; i++) {
        if (file_area_by_index(i, &rec, bbs_cfg.device_files) != BBS_OK) continue;
        if (rec.access_level > s->user.access_level) continue;
        sprintf(buf, " %2u  %-20s  %u FILES",
                (unsigned)i, rec.title, (unsigned)rec.total_files);
        fl(s, buf);
        found++;
    }
    if (!found) fl(s, " (NO FILE AREAS CONFIGURED)");
}

/* ── File listing within current area ───────────────────────────────────── */

static void fl_list_files(session_t *s, ud_area_record_t *area)
{
    file_entry_record_t fe;
    u8 recnum, count = 0;
    char buf[56];
    u8 pg = (s->term_rows > 6u) ? (u8)(s->term_rows - 4u) : 20u;
    u8 row = 2;

    { char hdr[28];
      sprintf(hdr, " AREA %u: %.18s", (unsigned)area->id, area->title);
      fl(s, hdr); }
    fl(s, "  #   FILENAME         BLKS DESCRIPTION");

    for (recnum = 1; recnum <= 255; recnum++) {
        if (!sess_carrier_ok(s)) break;
        if (fentry_by_recnum(area->id, recnum, &fe,
                             bbs_cfg.device_files) != BBS_OK) break;
        if (!fe.filename[0]) continue;
        if (fe.access_level > s->user.access_level) continue;
        sprintf(buf, "  %-3u %-16s %-4u %.24s",
                (unsigned)recnum, fe.filename,
                (unsigned)fe.blocks, fe.description);
        fl(s, buf);
        count++;
        if (++row >= pg) {
            u8 ch;
            ftx(s, "-- MORE (Q=STOP) --");
            while (!sess_read_key(s, &ch))
                if (!sess_carrier_ok(s)) { fnl(s); return; }
            fnl(s);
            if (ch == 'Q' || ch == 'q' || ch == 3 || ch == 27) return;
            row = 0;
        }
    }
    if (!count) fl(s, " (NO FILES IN THIS AREA)");
}

/* ── Download ───────────────────────────────────────────────────────────── */

static void fl_download(session_t *s, ud_area_record_t *area)
{
    file_entry_record_t fe;
    char input[5];
    u8 recnum, ch;
    punter_result_t pr;

    ftx(s, "FILE # (ENTER=CANCEL): ");
    sess_read_line(s, input, 4, TRUE);
    if (!input[0]) { fl(s, "CANCELLED."); return; }

    recnum = 0;
    { u8 i; for (i = 0; input[i] >= '0' && input[i] <= '9'; i++)
        recnum = (u8)(recnum * 10 + (input[i] - '0')); }

    if (!recnum ||
        fentry_by_recnum(area->id, recnum, &fe,
                         bbs_cfg.device_files) != BBS_OK ||
        !fe.filename[0]) { fl(s, "NOT FOUND."); return; }
    if (fe.access_level > s->user.access_level) { fl(s, "ACCESS DENIED."); return; }

    ftx(s, "PROTOCOL: (P)UNTER (Z)MODEM (ENTER=CANCEL): ");
    while (!sess_read_key(s, &ch)) if (!sess_carrier_ok(s)) return;
    fnl(s);
    if (ch == 'Z' || ch == 'z') {
        xfer_zmodem_send(s, area->device, bbs_cfg.drive_files, fe.filename);
        return;
    }
    if (ch != 'P' && ch != 'p') { fl(s, "CANCELLED."); return; }

    { char msg[36]; sprintf(msg, "PUNTER: %s", fe.filename); fl(s, msg); }
    fl(s, "BEGIN PUNTER RECEIVE NOW...");
    pr = punter_send(s, area->device, bbs_cfg.drive_files, fe.filename, 1);
    if (pr == PUNTER_OK) {
        fe.downloads++;
        fentry_save(area->id, &fe, bbs_cfg.device_files);
        fl(s, "TRANSFER COMPLETE.");
    } else { fl(s, "TRANSFER FAILED OR CANCELLED."); }
}

/* ── Upload ─────────────────────────────────────────────────────────────── */

static void fl_upload(session_t *s, ud_area_record_t *area)
{
    file_entry_record_t fe;
    char fname[16], desc[41];
    u8 key;
    punter_result_t pr;
    u8 filetype = 1;

    if (!bbs_cfg.allow_uploads) { fl(s, "UPLOADS DISABLED."); return; }
    if (area->upload_level > s->user.access_level) { fl(s, "ACCESS DENIED."); return; }

    ftx(s, "FILENAME (MAX 15 CHARS): ");
    sess_read_line(s, fname, 15, TRUE);
    if (!fname[0]) { fl(s, "CANCELLED."); return; }
    ftx(s, "DESCRIPTION: ");
    sess_read_line(s, desc, 40, FALSE);

    ftx(s, "PROTOCOL: (P)UNTER (Z)MODEM (ENTER=CANCEL): ");
    while (!sess_read_key(s, &key)) if (!sess_carrier_ok(s)) return;
    fnl(s);
    if (key == 'Z' || key == 'z') {
        xfer_zmodem_recv(s, area->device, bbs_cfg.drive_files, fname);
        return;
    }
    if (key != 'P' && key != 'p') { fl(s, "CANCELLED."); return; }

    { char msg[36]; sprintf(msg, "PUNTER UPLOAD: %s", fname); fl(s, msg); }
    fl(s, "BEGIN PUNTER SEND NOW...");
    pr = punter_recv(s, area->device, bbs_cfg.drive_files, fname, &filetype);

    if (pr == PUNTER_OK) {
        memset(&fe, 0, sizeof(fe));
        strncpy(fe.filename, fname, 15);
        strncpy(fe.description, desc, 40);
        strncpy(fe.uploader, s->handle, 15);
        fe.access_level = area->access_level;
        fentry_add(area->id, &fe, bbs_cfg.device_files);
        area->total_files++;
        file_area_save(area, bbs_cfg.device_files);
        s->user.uploads++;
        fl(s, "TRANSFER COMPLETE. FILE ADDED.");
    } else { fl(s, "TRANSFER FAILED OR CANCELLED."); }
}

/* ── Main dispatch (called by RESIDENT action_files) ───────────────────── */

void files_run(session_t *s)
{
    ud_area_record_t area;
    u8 area_idx = 0;
    u8 total;
    char cmd[5];

    total = file_area_count(bbs_cfg.device_files);

    /* Enter first accessible area */
    if (total && fl_load_area(s, 1, &area)) area_idx = 1;

    /* Header: m.files gfile or hardcoded fallback */
    if (session_display_file(s, 'm', "files") != BBS_OK) {
        fnl(s);
        fl(s, " FILE AREAS: (L)IST (D)OWN (U)P (A)REAS +/- # (?)HELP (Q)UIT");
    }

    for (;;) {
        if (!sess_carrier_ok(s)) break;

        /* MCI: set area name for %BN substitution in p.files */
        session_set_mci_board(area_idx ? area.title : "(NO AREA)");

        /* Prompt: p.files gfile or hardcoded fallback */
        if (session_display_file(s, 'p', "files") != BBS_OK) {
            if (area_idx) {
                char buf[28];
                sprintf(buf, " AREA %u: %.18s", (unsigned)area_idx, area.title);
                fl(s, buf);
            }
            ftx(s, " FILES CMD: ");
        }

        fgetkey_cmd(s, cmd);
        if (!sess_carrier_ok(s)) break;

        /* ENTER or L: list files in selected area */
        if (!cmd[0] || cmd[0] == 'L') {
            if (!area_idx) { fl(s, "NO AREA SELECTED."); continue; }
            fl_list_files(s, &area);
            continue;
        }

        /* A: list all accessible areas */
        if (cmd[0] == 'A') { fl_list_areas(s); continue; }

        /* +/> : next accessible area */
        if (cmd[0] == '+' || cmd[0] == '>') {
            fl_shift_area(s, TRUE, total, &area, &area_idx);
            continue;
        }

        /* -/< : previous accessible area */
        if (cmd[0] == '-' || cmd[0] == '<') {
            fl_shift_area(s, FALSE, total, &area, &area_idx);
            continue;
        }

        /* Digit: jump to area by number */
        if (cmd[0] >= '1' && cmd[0] <= '9') {
            u8 n = (u8)(cmd[0] - '0');
            if (cmd[1] >= '0' && cmd[1] <= '9')
                n = (u8)(n * 10 + (cmd[1] - '0'));
            if (fl_load_area(s, n, &area)) { area_idx = n; continue; }
            fl(s, "AREA NOT FOUND OR ACCESS DENIED.");
            continue;
        }

        /* D: download a file from current area */
        if (cmd[0] == 'D') {
            if (!area_idx) { fl(s, "NO AREA SELECTED."); continue; }
            fl_download(s, &area);
            continue;
        }

        /* U: upload a file to current area */
        if (cmd[0] == 'U') {
            if (!area_idx) { fl(s, "NO AREA SELECTED."); continue; }
            fl_upload(s, &area);
            continue;
        }

        /* ?: re-display menu header */
        if (cmd[0] == '?') {
            if (session_display_file(s, 'm', "files") != BBS_OK)
                fl(s, " (L)IST (D)OWN (U)P (A)REAS +/- # (Q)UIT");
            continue;
        }

        if (cmd[0] == 'Q') break;
    }
}
