/**
 * TURBO/64 BBS — Access Levels Module Header
 *
 * Per-level limits table stored in the "access" SEQ file (6 lines, one per
 * level 0–5). Whole-table load/save; see src/data/access.c for the format.
 */
#ifndef INCLUDE_BBS_ACCESS_H
#define INCLUDE_BBS_ACCESS_H

#include "types.h"
#include "err.h"
#include "records.h"

#define ACCESS_FILE        "ACCESS"   /* uppercase in code (matches cfg.c "CONFIG") */
#define ACCESS_LEVEL_COUNT 6

/* Fill out[] (6 entries) with the default seed limits for levels 0–5. */
void access_levels_defaults(access_level_t out[ACCESS_LEVEL_COUNT]);

/* Load the table from the "access" file on `device`.
 * out[] is first filled with defaults, then overlaid with any lines found, so
 * missing/partial files still yield a full valid table.
 * Returns BBS_OK if the file was read, BBS_ENOTFOUND if it does not exist
 * (out[] still holds defaults in that case). */
bbs_err_t access_levels_load(access_level_t out[ACCESS_LEVEL_COUNT], u8 device);

/* Load a single level's row by level id (0..5) into *out.
 * Convenience wrapper over access_levels_load for the runtime. Returns BBS_OK
 * (out populated, defaults if the file is absent) or BBS_EBADARG if level >= ACCESS_LEVEL_COUNT. */
bbs_err_t access_level_by_id(u8 level, access_level_t *out, u8 device);

/* Rewrite the "access" file on `device` with all 6 levels in level order. */
bbs_err_t access_levels_save(const access_level_t in[ACCESS_LEVEL_COUNT], u8 device);

/* Runtime-only: fetch one level's mins_per_day + flags without loading the
 * whole table. Returns BBS_OK if found, BBS_ENOTFOUND if file/level absent. */
bbs_err_t access_limits_runtime(u8 level, u16 *mins, u8 *flags, u8 device);

/* Fixed fallback label for a level (DELETED/NEW/USER/POWER/CO-SYSOP/SYSOP). */
const char *access_default_name(u8 level);

#endif /* INCLUDE_BBS_ACCESS_H */
