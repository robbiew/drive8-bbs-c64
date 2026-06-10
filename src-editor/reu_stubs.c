/* CONFIGURE REU stubs — messages.c references reu_index_get/put behind a
 * bbs_cfg.reu_enabled guard. The editor never enables REU, so these are
 * never called at runtime, but Oscar64 requires definitions to link. */
#include "bbs/hal/reu.h"

void reu_index_get(u16 msg_id, msg_index_record_t *out)
{
    (void)msg_id; (void)out;
}

void reu_index_put(u16 msg_id, const msg_index_record_t *rec)
{
    (void)msg_id; (void)rec;
}

/* users.c (shared DATA_SRCS) calls the data tier behind reu_data_available();
 * the editor has no REU, so available() is FALSE and put/get are never reached. */
bool_t reu_data_available(void) { return FALSE; }

void reu_data_put(u16 region_off, const void *src, u16 len)
{
    (void)region_off; (void)src; (void)len;
}

void reu_data_get(u16 region_off, void *dst, u16 len)
{
    (void)region_off; (void)dst; (void)len;
}
