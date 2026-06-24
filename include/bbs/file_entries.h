#ifndef INCLUDE_BBS_FILE_ENTRIES_H
#define INCLUDE_BBS_FILE_ENTRIES_H

#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/records.h"

/* Count non-deleted entries in the UD<area_id> REL file; stores result in *out_count. */
bbs_err_t fentry_count(u8 area_id, u8 device, u8 *out_count);

/* Load a specific file entry by 1-based record number within an area. */
bbs_err_t fentry_by_recnum(u8 area_id, u8 recnum, file_entry_record_t *out,
                            u8 device);

/* Append a new file entry to the area (fills next available slot). */
bbs_err_t fentry_add(u8 area_id, const file_entry_record_t *rec, u8 device);

/* Write an existing file entry back to disk (rec->id must be set). */
bbs_err_t fentry_save(u8 area_id, const file_entry_record_t *rec, u8 device);

/* Soft-delete an entry by clearing its filename. */
bbs_err_t fentry_delete(u8 area_id, u8 recnum, u8 device);

#endif /* INCLUDE_BBS_FILE_ENTRIES_H */
