/**
 * TURBO/64 BBS — Access Levels Data Module
 *
 * Reads/writes the "access" SEQ file: one comma-delimited line per level,
 *   "level,name,calls_per_day,mins_per_day,flags"
 * ascending by level. Mirrors src/data/cfg.c's disk_open/disk_gets/disk_puts
 * pattern (uppercase "ACCESS" name, drive 0). Whole-table load/save.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bbs/access.h"
#include "bbs/hal/disk.h"

static const char *const k_names[ACCESS_LEVEL_COUNT] = {
  "NO-ACCESS", "NEW", "USER", "POWER", "CO-SYSOP", "SYSOP"
};

const char *access_default_name(u8 level) {
  return (level < ACCESS_LEVEL_COUNT) ? k_names[level] : "";
}

void access_levels_defaults(access_level_t out[ACCESS_LEVEL_COUNT]) {
  static const u8  calls[ACCESS_LEVEL_COUNT] = { 0, 2, 5, 10, 50, 255 };
  static const u16 mins[ACCESS_LEVEL_COUNT]  = { 0, 20, 60, 120, 480, 1440 };
  static const u8  flags[ACCESS_LEVEL_COUNT] = {
    0,
    ACCESS_F_JOIN_POLLS,
    ACCESS_F_POST_ANON | ACCESS_F_PAGE_SYSOP | ACCESS_F_SEND_MAIL |
      ACCESS_F_JOIN_POLLS | ACCESS_F_UPLOAD,
    ACCESS_F_POST_ANON | ACCESS_F_PAGE_SYSOP | ACCESS_F_SEND_MAIL |
      ACCESS_F_JOIN_POLLS | ACCESS_F_UPLOAD,
    0x7F,   /* all except SYSOP */
    0xFF    /* all 8 */
  };
  u8 i;
  for (i = 0; i < ACCESS_LEVEL_COUNT; i++) {
    out[i].level = i;
    strncpy(out[i].name, k_names[i], sizeof(out[i].name) - 1);
    out[i].name[sizeof(out[i].name) - 1] = '\0';
    out[i].calls_per_day = calls[i];
    out[i].mins_per_day  = mins[i];
    out[i].flags         = flags[i];
  }
}

/* Advance p past the current field to the char after the next comma.
 * Returns NULL if no comma is found (malformed line). */
static const char *next_field(const char *p) {
  while (*p != '\0' && *p != ',') p++;
  if (*p == ',') return p + 1;
  return 0;
}

/* Parse one "lvl,name,calls,mins,flags" line into *r.
 * Parses in place via the const pointer (no scratch buffer) — atoi stops at
 * each comma, and the name is copied out directly. Returns FALSE if malformed.
 * NB: do NOT introduce a second large local buffer here; with the caller's
 * line[] also live, oscar64 can overlay the two and corrupt the input. */
static bool_t parse_line(const char *line, access_level_t *r) {
  const char *p = line;
  u8 i;

  if (*p < '0' || *p > '9') return FALSE;       /* field 0: level */
  r->level = (u8)atoi(p);

  p = next_field(p);                            /* field 1: name */
  if (!p) return FALSE;
  i = 0;
  while (*p != '\0' && *p != ',' && i < sizeof(r->name) - 1)
    r->name[i++] = *p++;
  r->name[i] = '\0';

  p = next_field(p);                            /* field 2: calls */
  if (!p) return FALSE;
  r->calls_per_day = (u8)atoi(p);

  p = next_field(p);                            /* field 3: mins */
  if (!p) return FALSE;
  r->mins_per_day = (u16)atoi(p);

  p = next_field(p);                            /* field 4: flags */
  if (!p) return FALSE;
  r->flags = (u8)atoi(p);
  return TRUE;
}

bbs_err_t access_levels_load(access_level_t out[ACCESS_LEVEL_COUNT], u8 device) {
  char line[64];
  i16 n;
  bbs_err_t err;

  access_levels_defaults(out);

  err = disk_open(device, 0, ACCESS_FILE, DISK_READ);
  if (err != BBS_OK) return BBS_ENOTFOUND;

  while ((n = disk_gets(line, sizeof(line))) > 0) {
    access_level_t rec;
    if (parse_line(line, &rec) && rec.level < ACCESS_LEVEL_COUNT) {
      out[rec.level] = rec;
    }
  }

  disk_close();
  return BBS_OK;
}

bbs_err_t access_levels_save(const access_level_t in[ACCESS_LEVEL_COUNT], u8 device) {
  bbs_err_t err;
  char line[64];
  u8 i;

  disk_scratch(device, 0, ACCESS_FILE);
  err = disk_open(device, 0, ACCESS_FILE, DISK_WRITE);
  if (err != BBS_OK) return err;

  for (i = 0; i < ACCESS_LEVEL_COUNT; i++) {
    sprintf(line, "%u,%s,%u,%u,%u\n",
            (unsigned)in[i].level, in[i].name,
            (unsigned)in[i].calls_per_day,
            (unsigned)in[i].mins_per_day,
            (unsigned)in[i].flags);
    disk_puts(line);
  }

  disk_close();
  return BBS_OK;
}

bbs_err_t access_level_by_id(u8 level, access_level_t *out, u8 device) {
  access_level_t tbl[ACCESS_LEVEL_COUNT];
  bbs_err_t rc;
  if (!out || level >= ACCESS_LEVEL_COUNT) return BBS_EBADARG;
  rc = access_levels_load(tbl, device);
  if (rc != BBS_OK && rc != BBS_ENOTFOUND) return rc;  /* propagate real I/O error */
  *out = tbl[level];
  return BBS_OK;
}

bbs_err_t access_limits_runtime(u8 level, u16 *mins, u8 *flags, u8 device) {
  char line[64];
  i16 n;
  bbs_err_t err;
  access_level_t rec;

  if (!mins || !flags) return BBS_EBADARG;

  err = disk_open(device, 0, ACCESS_FILE, DISK_READ);
  if (err != BBS_OK) return err;

  while ((n = disk_gets(line, sizeof(line))) > 0) {
    if (parse_line(line, &rec) && rec.level == level) {
      *mins  = rec.mins_per_day;
      *flags = rec.flags;
      disk_close();
      return BBS_OK;
    }
  }

  disk_close();
  return BBS_ENOTFOUND;
}
