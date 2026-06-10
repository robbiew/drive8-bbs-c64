#ifndef INCLUDE_BBS_VOTE_H
#define INCLUDE_BBS_VOTE_H
#include "types.h"
#include "err.h"
#include "session.h"
bbs_err_t vote_list(session_t *s);
bbs_err_t vote_cast(session_t *s);
#endif
