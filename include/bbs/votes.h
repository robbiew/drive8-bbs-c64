/**
 * TURBO/64 BBS — Vote/Poll Module Header
 *
 * Manages poll/vote records via REL files.
 */

#ifndef INCLUDE_BBS_VOTES_H
#define INCLUDE_BBS_VOTES_H

#include "types.h"
#include "err.h"
#include "records.h"

/**
 * vote_count()
 *
 * Count total non-deleted votes/polls.
 *
 * Returns count of votes with non-empty questions.
 */
u8 vote_count(u8 device);

/**
 * vote_by_index()
 *
 * Get the Nth non-deleted vote (n = 1 to vote_count).
 * Useful for admin listing and pagination.
 *
 * Args:
 *   n        — vote index (1-based)
 *   out_rec  — pointer to vote_record_t to populate
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — vote record loaded
 *   BBS_ENOTFOUND  — index out of range
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t vote_by_index(u8 n, vote_record_t *out_rec, u8 device);

/**
 * vote_by_id()
 *
 * Load a vote record by vote ID.
 *
 * Args:
 *   id       — vote ID (1–20)
 *   out_rec  — pointer to vote_record_t to populate
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — record loaded
 *   BBS_ENOTFOUND  — vote does not exist
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t vote_by_id(u8 id, vote_record_t *out_rec, u8 device);

/**
 * vote_save()
 *
 * Write a vote record to disk (update or insert).
 *
 * Args:
 *   rec      — pointer to vote_record_t to write
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — record saved
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t vote_save(const vote_record_t *rec, u8 device);

/**
 * vote_create()
 *
 * Create a new poll/vote question.
 * Assigns the next available vote ID (1–20).
 *
 * Args:
 *   question  — poll question (max 24 chars)
 *   device    — CBM device number
 *   out_id    — pointer to store assigned vote ID
 *
 * Returns:
 *   BBS_OK         — vote created
 *   BBS_EFULL      — vote table full (20 votes max)
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t vote_create(const char *question, u8 device, u8 *out_id);

/**
 * vote_delete()
 *
 * Soft-delete a vote by clearing the question (mark as deleted).
 *
 * Args:
 *   vote_id  — vote ID to delete
 *   device   — CBM device number
 *
 * Returns:
 *   BBS_OK         — vote deleted
 *   BBS_ENOTFOUND  — vote not found
 *   BBS_EIO        — disk I/O error
 */
bbs_err_t vote_delete(u8 vote_id, u8 device);

#endif /* INCLUDE_BBS_VOTES_H */
