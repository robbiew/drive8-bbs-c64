/* Telnet IAC filter for the single-line caller stream.
 * Strips IAC sequences from inbound bytes and drives TERMINAL-TYPE
 * negotiation (RFC 854 / RFC 1091). */
#ifndef TELNET_IAC_H
#define TELNET_IAC_H

#include "bbs/types.h"

#define TELNET_TERM_MAX   7   /* fits ANSI/VT100/VT220/xterm */
#define TELNET_REPLY_MAX 12   /* enough for initial DO/WILL negotiation */

typedef struct {
    u8 state;
    u8 sb_opt;
    u8 sb_buf[TELNET_TERM_MAX + 1];
    u8 sb_len;
    char term_type[TELNET_TERM_MAX + 1];
    u8 reply[TELNET_REPLY_MAX];
    u8 reply_len;
    bool_t sent_initial;
} telnet_filter_t;

void telnet_filter_init(telnet_filter_t *f);

/* Feed one inbound byte. Returns 1 and sets *out if it's an application
 * byte; returns 0 if consumed by the IAC state machine. */
u8 telnet_filter_feed(telnet_filter_t *f, u8 in, u8 *out);

/* Drain the reply queue into out (up to max bytes). Returns count. */
u8 telnet_filter_take_reply(telnet_filter_t *f, u8 *out, u8 max);

/* Returns the captured TERM string, or "" if not yet received. */
const char *telnet_filter_term(const telnet_filter_t *f);

#endif /* TELNET_IAC_H */
