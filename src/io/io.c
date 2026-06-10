/* bbs/io.h line editor implementation. */
#include "bbs/io.h"

void io_line_reset(io_line_t *l)
{
    l->len    = 0;
    l->buf[0] = '\0';
}

io_line_result_t io_line_feed(io_line_t *l, u8 in)
{
    if (in == 0x0D || in == 0x0A) {
        l->buf[l->len] = '\0';
        return IO_LINE_SUBMIT;
    }
    if (in == 0x08 || in == 0x7F) {
        if (l->len > 0) {
            l->len--;
            l->buf[l->len] = '\0';
            return IO_LINE_CONTINUE;
        }
        return IO_LINE_REJECT;
    }
    if (in < 0x20) return IO_LINE_REJECT;
    if (l->len >= IO_LINE_MAX) return IO_LINE_REJECT;
    l->buf[l->len++] = (char)in;
    l->buf[l->len]   = '\0';
    return IO_LINE_CONTINUE;
}
