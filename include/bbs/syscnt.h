#ifndef INCLUDE_BBS_SYSCNT_H
#define INCLUDE_BBS_SYSCNT_H

#include "types.h"
#include "err.h"

/* Load calls_today/posts_today from SYSCNT file into wfc global.
 * Resets both to 0 when the file's stamped date != wfc.date (new day),
 * is missing, or is in the legacy format. Must be called AFTER
 * wfc_set_datetime() so wfc.date is valid. */
void syscnt_load(void);

/* Persist wfc.calls_today and wfc.posts_today to SYSCNT file.
 * Call after incrementing the counters. */
bbs_err_t syscnt_save(void);

/* Create a fresh zeroed SYSCNT file on `device` (sentinel date forces a
 * reset + restamp on the first real boot). wfc-free so CONFIGURE can call it
 * from INIT FILES. */
bbs_err_t syscnt_init(u8 device);

#endif /* INCLUDE_BBS_SYSCNT_H */
