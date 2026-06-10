/* bbs/usrday.h - Per-user daily-limit state (USR.DAY REL file). */
#ifndef INCLUDE_BBS_USRDAY_H
#define INCLUDE_BBS_USRDAY_H

#include "types.h"
#include "err.h"
#include "records.h"

/* Load daily-limit state for user_id from USR.DAY.
 * Zeroes *out first, so a missing file/record yields an all-zero record.
 * Returns BBS_OK if read, BBS_ENOTFOUND if the file/record is absent,
 * or BBS_EBADARG. */
bbs_err_t usrday_load(u16 user_id, usr_day_record_t *out, u8 device);

/* Persist daily-limit state for user_id to USR.DAY (creates the file on first
 * write; REL auto-extends to the record). Returns BBS_OK or an error. */
bbs_err_t usrday_save(u16 user_id, const usr_day_record_t *rec, u8 device);

#endif /* INCLUDE_BBS_USRDAY_H */
