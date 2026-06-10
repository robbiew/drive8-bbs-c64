/**
 * TURBO/64 BBS — User Module Header
 *
 * Manages user record access via REL files.
 */

#ifndef INCLUDE_BBS_USERS_H
#define INCLUDE_BBS_USERS_H

#include "types.h"
#include "err.h"
#include "records.h"

/**
 * user_by_id()
 *
 * Load a user record by user ID.
 *
 * Args:
 *   id       — user ID (1–255)
 *   out_rec  — pointer to user_record_t to populate
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — record loaded
 *   BBS_ENOENT     — user does not exist
 *   BBS_EREAD      — disk read error
 */
bbs_err_t user_by_id(u8 id, user_record_t *out_rec, u8 device);

/**
 * user_by_handle()
 *
 * Search for a user by handle (username).
 * Scans the user REL file sequentially.
 *
 * Returns user ID if found, or 0 if not found.
 */
u8 user_by_handle(const char *handle, u8 device);

/**
 * user_save()
 *
 * Write a user record back to disk (update or insert).
 *
 * Returns:
 *   BBS_OK         — record saved
 *   BBS_EWRITE     — disk write error
 */
bbs_err_t user_save(const user_record_t *rec, u8 device);

/**
 * user_next_id()
 *
 * Find the next available user ID (scan for first free slot).
 * Returns 0 if user table is full (255 users).
 */
u8 user_next_id(u8 device);

/**
 * user_count()
 *
 * Count total non-deleted users in the database.
 *
 * Returns count of users with non-empty handles.
 */
u8 user_count(u8 device);

/* Bulk-load all user records into the REU data tier (Bank 2) for fast reads.
 * Call once at boot after USR LOG is verified. No-op if REU data tier absent
 * (reads then fall through to disk). Disk remains authoritative. */
void user_cache_load(u8 device);

/* TRUE if the REU user-record cache is populated and serving reads. */
bool_t user_cache_active(void);

/**
 * user_by_index()
 *
 * Get the Nth non-deleted user (n = 1 to user_count).
 * Useful for admin listing and pagination.
 *
 * Args:
 *   n        — user index (1-based)
 *   out_rec  — pointer to user_record_t to populate
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — user record loaded
 *   BBS_ENOTFOUND  — index out of range
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t user_by_index(u8 n, user_record_t *out_rec, u8 device);

/**
 * user_delete()
 *
 * Soft-delete a user by clearing the handle (mark as deleted).
 * The record still occupies space in the REL file.
 *
 * Args:
 *   user_id  — user ID to delete
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — user deleted
 *   BBS_ENOTFOUND  — user not found
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t user_delete(u8 user_id, u8 device);

/**
 * user_reset_password()
 *
 * Reset a user's password to a new value (sysop admin operation).
 * Password is hashed before storage.
 *
 * Args:
 *   user_id      — user ID to reset
 *   new_password — new password string (up to 4 chars)
 *   device       — CBM device number
 *
 * Returns:
 *   BBS_OK         — password updated
 *   BBS_ENOTFOUND  — user not found
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t user_reset_password(u8 user_id, const char *new_password, u8 device);

/**
 * user_hash_password()
 *
 * Hash a plaintext password (up to 4 chars) into the 4-byte form stored in
 * user_record_t.password. The single shared hash used by both the BBS
 * runtime and the editor — output bytes are always printable (0x21..0x7E),
 * never 0x00 or a control byte, so a stored hash is safe across the REL
 * read layer and any PETSCII/charset round-trip.
 *
 * Args:
 *   password  — plaintext (NUL-padded; only the first 4 bytes are used)
 *   out_hash  — 4-byte output buffer
 */
void user_hash_password(const char *password, char *out_hash);

/**
 * user_profile_save()
 *
 * Write a user profile record to "usr prof,L,86".
 *
 * Returns:
 *   BBS_OK    — saved
 *   BBS_EIO   — disk error
 */
bbs_err_t user_profile_save(const user_profile_record_t *rec, u8 device);

/**
 * user_profile_by_id()
 *
 * Load a user profile record by user ID.
 *
 * Returns:
 *   BBS_OK         — loaded
 *   BBS_ENOTFOUND  — no profile for this ID
 *   BBS_EIO        — disk error
 */
bbs_err_t user_profile_by_id(u8 id, user_profile_record_t *out_rec, u8 device);

#endif /* INCLUDE_BBS_USERS_H */
