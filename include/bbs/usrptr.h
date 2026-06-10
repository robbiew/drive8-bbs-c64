/* bbs/usrptr.h - Per-user message base read-state (USR.PTR REL file). */
#ifndef INCLUDE_BBS_USRPTR_H
#define INCLUDE_BBS_USRPTR_H

#include "types.h"
#include "err.h"
#include "records.h"

/* Load read-state for user_id from USR.PTR REL file.
 * Missing record = no read history; out is zero-initialised, returns BBS_OK. */
bbs_err_t usrptr_load(u16 user_id, usr_ptr_record_t *out, u8 device);

/* Persist read-state for user_id to USR.PTR REL file. */
bbs_err_t usrptr_save(u16 user_id, const usr_ptr_record_t *rec, u8 device);

/* Advance hwm[board_id-1] to max(current, msg_id). No disk I/O. */
void usrptr_advance(usr_ptr_record_t *ptr, u8 board_id, u16 msg_id);

#endif /* INCLUDE_BBS_USRPTR_H */
