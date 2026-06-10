/* Private to the term module. Per-mode byte translators. */
#ifndef TERM_INTERNAL_H
#define TERM_INTERNAL_H

#include "bbs/term.h"

u8 term_xlate_byte_petscii(u8 cp437, u8 *out, u8 max);
u8 term_xlate_byte_petscii_lower(u8 cp437, u8 *out, u8 max);
u8 term_xlate_byte_ascii  (u8 cp437, u8 *out, u8 max);

/* Streaming ANSI filter state (for future session_emit use). */
typedef struct {
    u8 mode;
    u8 state;
    u8 buf[16];
    u8 len;
} term_filter_t;

void term_filter_init(term_filter_t *f, term_mode_t mode);
u8   term_filter_feed(term_filter_t *f, u8 in, u8 *out, u8 max);

#endif /* TERM_INTERNAL_H */
