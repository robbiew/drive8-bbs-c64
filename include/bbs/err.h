/* bbs/err.h - uniform error codes used across all BBS modules. */
#ifndef BBS_ERR_H
#define BBS_ERR_H

#include "bbs/types.h"

typedef enum {
    BBS_OK        = 0,
    BBS_EIO       = 1,  /* I/O error (disk, ACIA) */
    BBS_EPERM     = 2,  /* permission denied / auth failure */
    BBS_EFULL     = 3,  /* buffer or table full */
    BBS_ENOTFOUND = 4,  /* file, user, message not found */
    BBS_EAGAIN    = 5,  /* try again — resource temporarily unavailable */
    BBS_EPROTO    = 6,  /* protocol error (telnet, record corruption) */
    BBS_EBADARG   = 7,  /* bad argument */
    BBS_EFATAL    = 8,  /* unrecoverable — BBS must halt */
    BBS_EEXIST    = 9,  /* already exists (duplicate user, etc.) */
    BBS_ENOTIMPL  = 10, /* not yet implemented */
    BBS_EQUIT     = 11  /* sysop requested graceful shutdown */
} bbs_err_t;

/* Returns a short static string for the given code. Never NULL. */
const char *bbs_err_str(bbs_err_t e);

#endif /* BBS_ERR_H */
