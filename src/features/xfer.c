/* xfer.c — Resident Zmodem transfer shim.  RESIDENT (no overlay pragma).
 *
 * Loads OVL_ZMODEM over the overlay zone, delegates to zmodem_send/recv,
 * then reloads OVL_FILES so the caller (fl_download / fl_upload in
 * OVL_FILES) can safely return.
 *
 * BSS note: oscar64's -Oo optimizer spills 76 bytes of activation records
 * into main BSS per krnio_load call when a zmodem function is also called
 * in the same scope.  Two mitigations here prevent overflow:
 *   1. z_load_zmodem / z_load_files are __noinline helpers so krnio_load
 *      runs in a separate scope from the zmodem_send/recv call.
 *   2. They take NO parameters — any parameter spilled across the krnio_load
 *      would add to main BSS.
 *
 * Safety: __noinline prevents -Oo from folding xfer_zmodem_send/recv into
 * OVL_FILES.  If inlined, krnio_load(OVL_ZMODEM) would execute from within
 * OVL_FILES at $9700, overwriting itself mid-call. */
#include "bbs/xfer.h"
#include "bbs/session.h"
#include "bbs/cfg.h"
#include "bbs/sysop.h"
#include "net/zmodem.h"
#include <c64/kernalio.h>

__noinline static void z_load_zmodem(void)
{
    krnio_setnam(P"OVL_ZMODEM");
    krnio_load(1, bbs_cfg.device_system, 1);
    wfc.ovl_wfc_loaded = FALSE;
}

__noinline static void z_load_files(void)
{
    krnio_setnam(P"OVL_FILES");
    krnio_load(1, bbs_cfg.device_system, 1);
    wfc.ovl_wfc_loaded = FALSE;
}

__noinline xfer_result_t xfer_zmodem_send(const session_t *s, u8 device, u8 drive,
                                            const char *filename)
{
    u8 r;
    z_load_zmodem();
    r = (u8)zmodem_send(s, device, drive, filename);
    z_load_files();
    if (r == ZMODEM_OK) return XFER_OK;
    if (r == ZMODEM_CANCEL) return XFER_CANCEL;
    return XFER_ERR;
}

__noinline xfer_result_t xfer_zmodem_recv(const session_t *s, u8 device, u8 drive,
                                            const char *filename)
{
    u8 r;
    z_load_zmodem();
    r = (u8)zmodem_recv(s, device, drive, filename);
    z_load_files();
    if (r == ZMODEM_OK) return XFER_OK;
    if (r == ZMODEM_CANCEL) return XFER_CANCEL;
    return XFER_ERR;
}
