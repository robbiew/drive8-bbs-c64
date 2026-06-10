/* bbs/err.h error code -> string table. */
#include "bbs/err.h"

const char *bbs_err_str(bbs_err_t e)
{
    switch (e) {
    case BBS_OK:        return "ok";
    case BBS_EIO:       return "io";
    case BBS_EPERM:     return "perm";
    case BBS_EFULL:     return "full";
    case BBS_ENOTFOUND: return "notfound";
    case BBS_EAGAIN:    return "again";
    case BBS_EPROTO:    return "proto";
    case BBS_EBADARG:   return "badarg";
    case BBS_EFATAL:    return "fatal";
    case BBS_EEXIST:    return "exist";
    default:            return "unknown";
    }
}
