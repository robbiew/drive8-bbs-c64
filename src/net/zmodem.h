#ifndef NET_ZMODEM_H
#define NET_ZMODEM_H

#include "bbs/types.h"
#include "bbs/session.h"

typedef enum {
    ZMODEM_OK     = 0,
    ZMODEM_ERR    = 1,
    ZMODEM_CANCEL = 2,
} zmodem_result_t;

/* Send a file via Zmodem (BBS → caller).  Stub: returns ZMODEM_ERR.
 * __noinline prevents the interprocedural optimizer from folding these into
 * the resident xfer.c shim, which would leave zmodem_code empty. */
__noinline zmodem_result_t zmodem_send(session_t *s, u8 device, u8 drive,
                                        const char *filename);

/* Receive a file via Zmodem (caller → BBS).  Stub: returns ZMODEM_ERR. */
__noinline zmodem_result_t zmodem_recv(session_t *s, u8 device, u8 drive,
                                        const char *filename);

#endif /* NET_ZMODEM_H */
