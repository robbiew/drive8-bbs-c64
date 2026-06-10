/* bbs/rel.h - CBM relative (record) file abstraction.
 *
 * GST BBS v6.0 uses REL files for all structured data:
 *   usr log  record=23  user records
 *   boards   record=40  bulletin board directory
 *   board<n> record=40  per-board message index
 *   uds      record=40  upload/download area directory
 *   ud<n>    record=100 per-area file entries
 *   text     record=25  help/text page index
 *   vote1    record=40  vote question/answer records
 *
 * On SD2IEC / 1571 / 1581: native CBM REL file support via drive DOS.
 * Positioning uses the "P" command on the command channel:
 *   PRINT#15, "P"; CHR$(data_sa); CHR$(rec_lo); CHR$(rec_hi); CHR$(0)
 *
 * Records are 1-based (CBM convention; record 0 is unused).
 */
#ifndef BBS_REL_H
#define BBS_REL_H

#include "bbs/types.h"
#include "bbs/err.h"

/* Opaque handle: encodes the logical file number used for this REL file. */
typedef u8 rel_handle_t;
#define REL_INVALID ((rel_handle_t)0xFF)

/* Open or create a REL file on `device`, path `name` (CBM filename
 * including drive prefix, e.g. "0:USR LOG").
 * `record_size`: 1-254 bytes.
 * If the file does not exist it is created; if it exists the record
 * size must match. Returns BBS_OK on success. */
bbs_err_t rel_open(u8 device, const char *name, u8 record_size,
                   rel_handle_t *out);

/* Position to record `rec` (1-based). Must be called before rel_read
 * or rel_write. */
bbs_err_t rel_position(rel_handle_t h, u16 rec);

/* Read one record (record_size bytes) from the current position into
 * `buf`. `*got` is set to the number of bytes actually read. */
bbs_err_t rel_read(rel_handle_t h, void *buf, u8 record_size, u8 *got);

/* Write one record (record_size bytes) at the current position. */
bbs_err_t rel_write(rel_handle_t h, const void *buf, u8 record_size);

/* Close the REL file handle. */
bbs_err_t rel_close(rel_handle_t h);

/* Force-clear the single-open guard when the underlying KERNAL channels
 * were closed by external code (e.g. after a disk_scratch + raw krnio_close). */
void rel_reset(void);

#endif /* BBS_REL_H */
