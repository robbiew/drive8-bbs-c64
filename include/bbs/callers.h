#ifndef INCLUDE_BBS_CALLERS_H
#define INCLUDE_BBS_CALLERS_H
#include "types.h"
#include "err.h"
#include "session.h"
#include "sysop.h"

/* On-disk filename and minimum line length (for raw parsing) */
#define CALLERS_FILE     "CALLERS"
#define CALLERS_LINE_MIN 35

/**
 * callers_log()
 * Append a completed session to the on-disk CALLERS log file.
 * Full format: "MM/DD HH:MM A HHHHHHHHHHHHHH BBBBB DDDDD" (41 chars + CR).
 * Old files with the 35-char format are still accepted by callers_load().
 * elapsed_secs: session length in seconds (from clock_elapsed).
 * Call after session_done() before wfc_display().
 */
bbs_err_t callers_log(session_t *s, u16 elapsed_secs);

/**
 * callers_load()
 * Read the CALLERS log from disk and populate `buf` with the last `max`
 * entries in chronological order (oldest first).  *got is set to the
 * number of entries written (0 if file not found or empty).
 * Call from wfc_init() to restore the activity log across reboots.
 */
void callers_load(wfc_log_entry_t *buf, u8 max, u8 *got);

#endif
