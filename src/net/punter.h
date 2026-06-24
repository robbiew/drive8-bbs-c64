#ifndef NET_PUNTER_H
#define NET_PUNTER_H

#include "bbs/types.h"
#include "bbs/session.h"

typedef enum {
    PUNTER_OK     = 0,
    PUNTER_ERR    = 1,
    PUNTER_CANCEL = 2,
} punter_result_t;

/* Send `filename` from `device`/`drive` to the caller via Punter.
 * filetype: 0=PRG, 1=SEQ.  Lives in OVL_FILES. */
punter_result_t punter_send(session_t *s, u8 device, u8 drive,
                             const char *filename, u8 filetype);

/* Receive a file from the caller via Punter; write to `device`/`drive`
 * as `filename`.  `out_filetype` receives the peer's reported type byte.
 * Lives in OVL_FILES. */
punter_result_t punter_recv(session_t *s, u8 device, u8 drive,
                             const char *filename, u8 *out_filetype);

#endif /* NET_PUNTER_H */
