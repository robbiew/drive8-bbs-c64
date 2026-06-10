/* TURBO/64 BBS — New user registration state machine step handler */
#ifndef INCLUDE_BBS_NEWUSER_H
#define INCLUDE_BBS_NEWUSER_H

#include "types.h"
#include "session.h"

/* Process one character in the SESS_REGISTERING state machine. */
void session_reg_step(session_t *s, u8 ch);

#endif /* INCLUDE_BBS_NEWUSER_H */
