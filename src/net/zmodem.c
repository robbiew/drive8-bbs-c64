/* zmodem.c — Zmodem transfer stub (OVL_ZMODEM section).
 * Full implementation is a future milestone.
 * Non-empty bodies are required to prevent oscar64 from eliminating the
 * functions and generating a corrupt (zero-byte) overlay PRG. */
#include "bbs/overlay.h"
#include "bbs/session.h"
#include "net/zmodem.h"

#pragma code(zmodem_code)
#pragma data(zmodem_data)
#pragma bss(zmodem_bss)

/* Force native 6502 compilation to keep these in zmodem_code rather than
 * letting the bytecode optimizer inline them into the resident section. */
#pragma native(zmodem_send)
#pragma native(zmodem_recv)

__noinline zmodem_result_t zmodem_send(session_t *s, u8 device, u8 drive,
                                        const char *filename)
{
    (void)device; (void)drive; (void)filename;
    session_emit(s, "\r\nZMODEM: NOT YET IMPLEMENTED.\r\n");
    return ZMODEM_ERR;
}

__noinline zmodem_result_t zmodem_recv(session_t *s, u8 device, u8 drive,
                                        const char *filename)
{
    (void)device; (void)drive; (void)filename;
    session_emit(s, "\r\nZMODEM: NOT YET IMPLEMENTED.\r\n");
    return ZMODEM_ERR;
}
