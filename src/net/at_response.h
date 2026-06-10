/* AT response parser for the U64 emulated modem's status messages.
 * Once CONNECT is seen, all subsequent bytes are raw caller payload. */
#ifndef AT_RESPONSE_H
#define AT_RESPONSE_H

#include "bbs/types.h"

typedef enum {
    AT_EVT_NONE      = 0,
    AT_EVT_OK        = 1,
    AT_EVT_RING      = 2,
    AT_EVT_CONNECT   = 3,
    AT_EVT_NOCARRIER = 4,
    AT_EVT_ERROR     = 5
} at_event_t;

typedef struct {
    u8     line[15];  /* max AT response: "CONNECT 115200" = 14 chars */
    u8     line_len;
    bool_t connected;
} at_parser_t;

void at_parser_init(at_parser_t *p);

/* Feed one byte. If it's an application byte (post-CONNECT), sets
 * *out_byte and *out_app=1; otherwise *out_app=0. Returns event. */
at_event_t at_parser_feed(at_parser_t *p, u8 in, u8 *out_byte, u8 *out_app);

#endif /* AT_RESPONSE_H */
