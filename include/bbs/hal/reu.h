/* bbs/hal/reu.h - RAM Expansion Unit (REU) detection and DMA API.
 *
 * Bank 0 (64KB): active board index (B<n>.IDX loaded here at board-enter)
 * Bank 1 (64KB): compose buffer (message being written)
 * Bank 2 (64KB): generic data tier (reu_data_*, named regions)
 * Banks 3+     : opportunistic body cache
 *
 * All functions degrade gracefully when reu_present() == FALSE:
 * callers must check bbs_cfg.reu_enabled before using DMA paths.
 */
#ifndef BBS_HAL_REU_H
#define BBS_HAL_REU_H

#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/records.h"

/* Detection — call once at boot. Returns detected size in KB (0 = absent);
 * also stores it in bbs_cfg.reu_detected_size / reu_enabled. */
u16 reu_detect(void);
u8     reu_bank_count(void);  /* number of 64KB banks available (2, 4, or 8) */

/* Bank 2: generic data tier --------------------------------------------
 * Named regions at fixed offsets within REU Bank 2. Coexists with the index
 * (bank 0), compose (bank 1) and body cache (banks 3+). Requires >=3 banks;
 * callers MUST gate on reu_data_available() and keep a disk fallback. */
#define REU_REGION_USERS   0x0000u   /* user-record cache: user n at +(n-1)*RECORD_SIZE_USER */
/* future regions start at higher offsets, e.g. 0x2000 = REU_REGION_MAIL */

bool_t reu_data_available(void);                          /* reu_enabled && bank_count >= 3 */
void   reu_data_put(u16 region_off, const void *src, u16 len);
void   reu_data_get(u16 region_off, void *dst, u16 len);

/* Bank 0: active board message index --------------------------------- */

/* DMA entire B<n>.IDX REL file from disk into REU Bank 0.
 * Reads records sequentially, DMAs each 32-byte record to offset
 * (msg_id - 1) * RECORD_SIZE_MSG_IDX within Bank 0.
 * Returns BBS_OK on success; BBS_EIO on disk error. */
bbs_err_t reu_index_load(u8 board_id, u8 device);

/* DMA REU Bank 0 back to B<n>.IDX REL file on disk.
 * Writes all records up to the board's current msg_count.
 * Call after msg_post() to persist the updated index. */
bbs_err_t reu_index_flush(u8 board_id, u8 device);

/* Read one index record from REU Bank 0 (zero disk I/O).
 * msg_id is 1-based; reads from offset (msg_id-1)*32 in Bank 0. */
void reu_index_get(u16 msg_id, msg_index_record_t *out);

/* Write one index record to REU Bank 0 (marks dirty; flush with reu_index_flush). */
void reu_index_put(u16 msg_id, const msg_index_record_t *rec);

/* Bank 1: compose buffer --------------------------------------------- */

void      reu_compose_init(void);
void      reu_compose_putc(char c);
void      reu_compose_puts(const char *s);
u16       reu_compose_len(void);
void      reu_compose_truncate(u16 len);

/* Read bytes back from compose buffer (REU Bank 1).
 * Reads min(len, compose_len - offset) bytes into buf. */
void reu_compose_read(u16 offset, char *buf, u16 len);

/* Append compose buffer contents to B<n>.TXT SEQ file on disk.
 * out_offset: caller-provided current EOF position (see msg_body_eof);
 *             read-only — 0 selects WRITE, non-zero selects APPEND.
 * out_len: set to number of bytes written.
 * Clears compose buffer after successful commit. */
bbs_err_t reu_compose_commit(u8 board_id, u8 device,
                              const u16 *out_offset, u16 *out_len);

/* Banks 2+: body cache ----------------------------------------------- */

bool_t    reu_body_cached(u8 board_id, u16 msg_id);
void      reu_body_store(u8 board_id, u16 msg_id, const char *buf, u16 len);
bbs_err_t reu_body_fetch(u8 board_id, u16 msg_id, char *buf, u16 buf_len);
bbs_err_t reu_body_fetch_at(u8 board_id, u16 msg_id,
                              char *buf, u8 len, u16 offset);

#endif /* BBS_HAL_REU_H */
