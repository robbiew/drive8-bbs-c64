/* xfer.c — Resident Zmodem transfer shim.  RESIDENT (no overlay pragma).
 *
 * Stub phase: these functions print "NOT YET IMPLEMENTED" directly.
 * When Zmodem is fully implemented, replace with the overlay-switching
 * pattern: load OVL_ZMODEM, call zmodem_send/recv, reload OVL_FILES.
 *
 * NOTE: Do NOT load OVL_ZMODEM from within these functions while they are
 * called from OVL_FILES code — loading another overlay destroys OVL_FILES
 * at $9700 while it is still executing.  The overlay-switch pattern is
 * safe only when these are RESIDENT and the optimizer does not inline them
 * into the overlay.  For the stub, no overlay switch is needed. */
#include "bbs/xfer.h"
#include "bbs/session.h"

xfer_result_t xfer_zmodem_send(session_t *s, u8 device, u8 drive,
                                const char *filename)
{
    (void)device; (void)drive; (void)filename;
    session_emit(s, "\r\nZMODEM: NOT YET IMPLEMENTED.\r\n");
    return XFER_ERR;
}

xfer_result_t xfer_zmodem_recv(session_t *s, u8 device, u8 drive,
                                const char *filename)
{
    (void)device; (void)drive; (void)filename;
    session_emit(s, "\r\nZMODEM: NOT YET IMPLEMENTED.\r\n");
    return XFER_ERR;
}
