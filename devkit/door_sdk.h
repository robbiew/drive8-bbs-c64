/* T/64 Door SDK — include this header in your door. */
#ifndef T64_DOOR_SDK_H
#define T64_DOOR_SDK_H

#include "bbs/door_abi.h"

/* bbs() — read the Door Control Block at $033C and return the host API pointer.
 * Returns NULL unless all three DCB fields validate (door launched outside a
 * T/64 BBS, or an ABI mismatch) — the cassette buffer isn't zeroed by reset, so
 * check magic0+magic1+version, not just one byte. */
static inline const bbs_api_t *bbs(void) {
    volatile u8 *dcb = (volatile u8 *)BBS_DCB_ADDR;
    if (!door_abi_check(dcb[BBS_DCB_MAGIC0], dcb[BBS_DCB_MAGIC1], dcb[BBS_DCB_VER]))
        return 0;
    return (const bbs_api_t *)((u16)dcb[BBS_DCB_PTR_LO]
                             | ((u16)dcb[BBS_DCB_PTR_HI] << 8));
}

#endif
