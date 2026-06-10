#ifndef INCLUDE_BBS_MAIL_H
#define INCLUDE_BBS_MAIL_H
#include "types.h"
#include "err.h"
#include "session.h"
bbs_err_t mail_list(session_t *s);
bbs_err_t mail_read(session_t *s);
bbs_err_t mail_send(session_t *s);
#endif
