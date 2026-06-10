/* bbs/editor.h - Line editor for message compose. */
#ifndef INCLUDE_BBS_EDITOR_H
#define INCLUDE_BBS_EDITOR_H

#include "bbs/session.h"

typedef enum {
  EDITOR_SAVE,
  EDITOR_ABORT,
  EDITOR_QUOTE,           /* /Q typed — caller should insert quote then re-enter */
  EDITOR_CONTINUE = 0xFF  /* sentinel: command handled, keep editing */
} editor_result_t;

/* When context_line is non-NULL the editor opens with the old-style
 * subject echo + help banner.  Pass NULL when the caller has already
 * drawn the full header and only a bare "> " prompt is needed. */
editor_result_t editor_run(const session_t *s, const char *context_line);

#endif /* INCLUDE_BBS_EDITOR_H */
