/* bbs/bulletin.h - Message board feature layer. */
#ifndef INCLUDE_BBS_BULLETIN_H
#define INCLUDE_BBS_BULLETIN_H

#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/session.h"

bbs_err_t bulletin_run(session_t *s);

#endif /* INCLUDE_BBS_BULLETIN_H */
