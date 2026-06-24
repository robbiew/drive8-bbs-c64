#ifndef INCLUDE_BBS_XFER_H
#define INCLUDE_BBS_XFER_H

#include "bbs/types.h"
#include "bbs/session.h"

/* Transfer result */
typedef enum {
    XFER_OK     = 0,
    XFER_ERR    = 1,
    XFER_CANCEL = 2,
} xfer_result_t;

/* RESIDENT: Load OVL_ZMODEM, run Zmodem send, reload OVL_FILES.
 * Called from within OVL_FILES when a caller requests Zmodem download.
 * After zmodem_send() returns, OVL_FILES is reloaded so the caller can
 * continue executing (mirrors the door_run() pattern). */
xfer_result_t xfer_zmodem_send(const session_t *s, u8 device, u8 drive,
                                const char *filename);

/* RESIDENT: Load OVL_ZMODEM, run Zmodem receive, reload OVL_FILES. */
xfer_result_t xfer_zmodem_recv(const session_t *s, u8 device, u8 drive,
                                const char *filename);

#endif /* INCLUDE_BBS_XFER_H */
