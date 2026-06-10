/**
 * TURBO/64 BBS — Message Base Data Layer
 *
 * Manages per-board message index (B<n>.IDX REL) and bodies (B<n>.TXT SEQ).
 * REU-transparent: uses REU Bank 0 when bbs_cfg.reu_enabled, else direct disk.
 */
#ifndef INCLUDE_BBS_MESSAGES_H
#define INCLUDE_BBS_MESSAGES_H

#include "types.h"
#include "err.h"
#include "records.h"

/* ---- Index CRUD --------------------------------------------------- */

bbs_err_t msg_index_get(u8 board_id, u16 msg_id,
                         msg_index_record_t *out, u8 device);
bbs_err_t msg_index_put(u8 board_id, const msg_index_record_t *rec, u8 device);
/* Count total + deleted index records in one open/close pass (fast). */
bbs_err_t msg_index_stats(u8 board_id, u8 device, u16 *out_total, u16 *out_deleted);

/* ---- Post --------------------------------------------------------- */

/* date_mmddyy: current system date as "MM/DD/YY" (e.g. wfc.date); stored as
 * BCD month/day/year in the index record.  NULL/empty leaves the date unset. */
bbs_err_t msg_post(u8 board_id, u16 parent_id, u16 author_id, u16 to_id,
                   bool_t anonymous, u8 device, u16 *out_msg_id,
                   const char *subj, const char *date_mmddyy);

typedef void (*msg_line_cb_t)(const char *line, void *ctx);
bbs_err_t msg_body_each_line(u8 board_id, const msg_index_record_t *rec,
                              msg_line_cb_t cb, void *ctx, u8 device);

/* ---- Body --------------------------------------------------------- */

bbs_err_t msg_body_read(u8 board_id, const msg_index_record_t *rec,
                         char *buf, u16 buf_len, u8 device);

/* ---- Flag operations --------------------------------------------- */

bbs_err_t msg_delete(u8 board_id, u16 msg_id, u8 device);

/* ---- Scan -------------------------------------------------------- */

u8  msg_count_new(u8 board_id, u16 hwm, u16 last_call_date, u8 device);
u16 msg_next_unread_any(u8 board_id, u16 hwm, u16 last_call_date, u8 device);

/* ---- Prune ------------------------------------------------------- */

bbs_err_t msg_prune_quantity(u8 board_id, u8 device);

/* ---- Compaction (CONFIGURE only) ------------------------------------ */

bbs_err_t msg_compact(u8 board_id, u8 device);

#endif /* INCLUDE_BBS_MESSAGES_H */
