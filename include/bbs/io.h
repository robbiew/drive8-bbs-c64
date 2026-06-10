/* bbs/io.h - per-session line editor. */
#ifndef BBS_IO_H
#define BBS_IO_H

#include "bbs/types.h"

#define IO_LINE_MAX 254   /* leaves room for NUL terminator */

typedef struct {
    char buf[IO_LINE_MAX + 1];
    u8   len;
} io_line_t;

typedef enum {
    IO_LINE_CONTINUE = 0,   /* more input expected */
    IO_LINE_SUBMIT   = 1,   /* line complete (Enter pressed) */
    IO_LINE_REJECT   = 2    /* byte not accepted (overflow, bad control) */
} io_line_result_t;

void             io_line_reset(io_line_t *l);
io_line_result_t io_line_feed (io_line_t *l, u8 in);

#endif /* BBS_IO_H */
