#ifndef INCLUDE_BBS_FILES_H
#define INCLUDE_BBS_FILES_H
#include "types.h"
#include "err.h"
#include "session.h"
bbs_err_t files_list(session_t *s);
bbs_err_t files_download(session_t *s);
bbs_err_t files_upload(session_t *s);
#endif
