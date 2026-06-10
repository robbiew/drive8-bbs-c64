/* src/data/usrptr.c - User read-state management. */

#include "bbs/usrptr.h"
#include "bbs/cfg.h"
#include "bbs/rel.h"
#include "bbs/config.h"
#include <string.h>
#include <stdio.h>
#include "bbs/overlay.h"

#define RECORD_READ_MIN 2
#pragma code(msgs_code)
#pragma data(msgs_data)
#pragma bss(msgs_bss)

static bbs_err_t usrptr_open(u8 device, rel_handle_t *h)
{
    char fname[32];
    bbs_err_t err;
    err = cfg_send_drive_init(device, bbs_cfg.init_msgs);
    if (err != BBS_OK) return err;
    sprintf(fname, "%u:USR.PTR", (unsigned)bbs_cfg.drive_msgs);
    return rel_open(device, fname, RECORD_SIZE_USR_PTR, h);
}

bbs_err_t usrptr_load(u16 user_id, usr_ptr_record_t *out, u8 device)
{
    rel_handle_t h;
    bbs_err_t err;
    u8 buf[RECORD_SIZE_USR_PTR];
    u8 got, i;

    if (!out || user_id == 0) return BBS_EBADARG;

    memset(out, 0, sizeof(*out));  /* default: no read history */

    err = usrptr_open(device, &h);
    if (err != BBS_OK) return err;

    err = rel_position(h, (u8)user_id);
    if (err != BBS_OK) { rel_close(h); return err; }

    memset(buf, 0, RECORD_SIZE_USR_PTR);
    err = rel_read(h, buf, RECORD_SIZE_USR_PTR, &got);
    rel_close(h);

    /* Missing record = user has never read messages. Not an error. */
    if (err != BBS_OK || got < RECORD_READ_MIN) {
        return BBS_OK;
    }
    if (got < RECORD_SIZE_USR_PTR) {
        memset(buf + got, 0, RECORD_SIZE_USR_PTR - got);
    }

    for (i = 0; i < CFG_MAX_BOARDS; i++) {
        out->hwm[i] = (u16)buf[i * 2] | ((u16)buf[i * 2 + 1] << 8);
    }
    return BBS_OK;
}

bbs_err_t usrptr_save(u16 user_id, const usr_ptr_record_t *rec, u8 device)
{
    rel_handle_t h;
    bbs_err_t err;
    u8 buf[RECORD_SIZE_USR_PTR];
    u8 i;

    if (!rec || user_id == 0) return BBS_EBADARG;

    memset(buf, 0, RECORD_SIZE_USR_PTR);
    for (i = 0; i < CFG_MAX_BOARDS; i++) {
        buf[i * 2]     = (u8)(rec->hwm[i] & 0xFF);
        buf[i * 2 + 1] = (u8)(rec->hwm[i] >> 8);
    }

    err = usrptr_open(device, &h);
    if (err != BBS_OK) return err;

    err = rel_position(h, (u8)user_id);
    if (err != BBS_OK) { rel_close(h); return err; }

    err = rel_write(h, buf, RECORD_SIZE_USR_PTR);
    rel_close(h);
    return err;
}

void usrptr_advance(usr_ptr_record_t *ptr, u8 board_id, u16 msg_id)
{
    u8 idx;
    if (!ptr || board_id == 0 || board_id > CFG_MAX_BOARDS) return;
    idx = board_id - 1;
    if (msg_id > ptr->hwm[idx]) ptr->hwm[idx] = msg_id;
}
#pragma code(code)
#pragma data(data)
#pragma bss(bss)
