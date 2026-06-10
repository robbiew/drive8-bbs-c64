/**
 * TURBO/64 BBS — File Upload/Download Area Module Header
 *
 * Manages file area (upload/download directory) records via REL files.
 */

#ifndef INCLUDE_BBS_FILE_AREAS_H
#define INCLUDE_BBS_FILE_AREAS_H

#include "types.h"
#include "err.h"
#include "records.h"

/**
 * file_area_count()
 *
 * Count total non-deleted file areas.
 *
 * Returns count of areas with non-empty titles.
 */
u8 file_area_count(u8 device);

/**
 * file_area_by_index()
 *
 * Get the Nth non-deleted file area (n = 1 to file_area_count).
 * Useful for admin listing and pagination.
 *
 * Args:
 *   n        — area index (1-based)
 *   out_rec  — pointer to ud_area_record_t to populate
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — area record loaded
 *   BBS_ENOTFOUND  — index out of range
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t file_area_by_index(u8 n, ud_area_record_t *out_rec, u8 device);

/**
 * file_area_by_id()
 *
 * Load a file area record by area ID.
 *
 * Args:
 *   id       — area ID (1–8)
 *   out_rec  — pointer to ud_area_record_t to populate
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — record loaded
 *   BBS_ENOTFOUND  — area does not exist
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t file_area_by_id(u8 id, ud_area_record_t *out_rec, u8 device);

/**
 * file_area_save()
 *
 * Write a file area record to disk (update or insert).
 *
 * Args:
 *   rec      — pointer to ud_area_record_t to write
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — record saved
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t file_area_save(const ud_area_record_t *rec, u8 device);

/**
 * file_area_create()
 *
 * Create a new file upload/download area.
 * Assigns the next available area ID (1–8).
 *
 * Args:
 *   title           — area name (max 20 chars)
 *   access_level    — minimum access level to view/download
 *   upload_level    — minimum access level to upload
 *   device          — CBM device number
 *   out_id          — pointer to store assigned area ID
 *
 * Returns:
 *   BBS_OK         — area created
 *   BBS_EFULL      — area table full (8 areas max)
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t file_area_create(const char *title, u8 access_level, u8 upload_level,
                           u8 device, u8 *out_id);

/**
 * file_area_delete()
 *
 * Soft-delete a file area by clearing the title (mark as deleted).
 *
 * Args:
 *   area_id  — area ID to delete
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — area deleted
 *   BBS_ENOTFOUND  — area not found
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t file_area_delete(u8 area_id, u8 device);

#endif /* INCLUDE_BBS_FILE_AREAS_H */
