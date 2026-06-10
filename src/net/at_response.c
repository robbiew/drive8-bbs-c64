/* AT response parser implementation. Ported from drive8-bbs. */
#include "at_response.h"
#include <string.h>

void at_parser_init(at_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

static bool_t line_eq(const at_parser_t *p, const char *s)
{
    u8 n = (u8)strlen(s);
    return (p->line_len == n && memcmp(p->line, s, n) == 0) ? TRUE : FALSE;
}

static at_event_t classify_line(const at_parser_t *p)
{
    if (line_eq(p, "OK"))         return AT_EVT_OK;
    if (line_eq(p, "RING"))       return AT_EVT_RING;
    if (line_eq(p, "ERROR"))      return AT_EVT_ERROR;
    if (line_eq(p, "NO CARRIER")) return AT_EVT_NOCARRIER;
    if (p->line_len >= 7 && memcmp(p->line, "CONNECT", 7) == 0)
        return AT_EVT_CONNECT;
    return AT_EVT_NONE;
}

at_event_t at_parser_feed(at_parser_t *p, u8 in, u8 *out_byte, u8 *out_app)
{
    if (p->connected) {
        /* Pass the byte through as session data, but keep scanning the line
         * buffer for the NO CARRIER result code.  Under VICE/tcpser there is no
         * DSR carrier line, so an in-band NO CARRIER is the only hangup signal
         * once connected — without this the session never ends and the BBS
         * stays stuck "connected". */
        *out_byte = in;
        *out_app  = 1;
        if (in == '\r' || in == '\n') {
            at_event_t e = (p->line_len > 0) ? classify_line(p) : AT_EVT_NONE;
            p->line_len = 0;
            if (e == AT_EVT_NOCARRIER) {
                p->connected = FALSE;
                return AT_EVT_NOCARRIER;
            }
            return AT_EVT_NONE;   /* ignore other result codes while connected */
        }
        if (p->line_len < (u8)(sizeof(p->line) - 1))
            p->line[p->line_len++] = in;
        return AT_EVT_NONE;
    }

    *out_app = 0;
    if (in == '\r' || in == '\n') {
        if (p->line_len == 0) return AT_EVT_NONE;
        at_event_t e = classify_line(p);
        p->line_len = 0;
        if (e == AT_EVT_CONNECT) p->connected = TRUE;
        return e;
    }
    if (p->line_len < (u8)(sizeof(p->line) - 1))
        p->line[p->line_len++] = in;
    return AT_EVT_NONE;
}
