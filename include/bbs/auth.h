/**
 * TURBO/64 BBS — User Authentication Module
 *
 * Handles login, new user registration, password validation, access level checks.
 */

#ifndef INCLUDE_BBS_AUTH_H
#define INCLUDE_BBS_AUTH_H

#include "types.h"
#include "err.h"
#include "records.h"
#include "session.h"

/**
 * auth_prompt_login()
 *
 * Prompt user for handle/password and authenticate.
 * Populates session->user and session->user_id on success.
 *
 * Returns:
 *   BBS_OK         — user authenticated
 *   BBS_EPERM      — bad password / access denied
 *   BBS_ENOTFOUND  — user not found (suggest new user)
 */
bbs_err_t auth_prompt_login(session_t *s);

/**
 * auth_register_new()
 *
 * New user self-registration flow.
 * Prompts for handle, password, demographics; creates user record.
 * Assigns new user ID and access level from config.
 *
 * Returns:
 *   BBS_OK         — user created and authenticated
 *   BBS_EFULL      — user table full (255 users)
 *   BBS_EEXIST     — handle already taken
 */
bbs_err_t auth_register_new(session_t *s);

/**
 * auth_validate_password()
 *
 * Check if provided password matches user's stored password.
 * Implements simple password hashing (CBM charset).
 *
 * Returns:
 *   BBS_OK    — password correct
 *   BBS_EPERM — password incorrect
 */
bbs_err_t auth_validate_password(const char *handle, const char *password_attempt);

/**
 * auth_check_access()
 *
 * Verify user's access level for a feature (e.g., upload level, sysop area).
 *
 * Returns:
 *   BBS_OK    — user has access
 *   BBS_EPERM — access denied
 */
bbs_err_t auth_check_access(const user_record_t *user, u8 min_level);

/**
 * auth_validate_handle()
 *
 * Validate a handle/username for signup according to BBS rules:
 * - Minimum 2 characters
 * - Maximum 15 characters
 * - Must be unique (not already in use)
 * - Cannot be a reserved name (e.g., "sysop")
 * - Cannot be only digits
 *
 * Returns:
 *   BBS_OK     — handle is valid
 *   BBS_EBADARG — handle too short/long or invalid characters
 *   BBS_EEXIST — handle already taken or reserved
 */
bbs_err_t auth_validate_handle(const char *handle, u8 device);

#endif /* INCLUDE_BBS_AUTH_H */
