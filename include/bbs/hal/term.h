/**
 * TURBO/64 BBS — Terminal Detection & Capability Negotiation (HAL)
 *
 * Public API for terminal type detection at connect time.
 * User selects from a menu (1–3); width and ANSI caps are set automatically:
 *   PETSCII  → 40 col, no ANSI
 *   ANSI     → 80 col, CP437, color + IBM graphics
 *   ASCII    → 80 col
 *
 * Results stored in session_t fields:
 *   term_mode, term_width, ansi_color, ansi_graphics, linefeed_mode
 */

#ifndef INCLUDE_BBS_HAL_TERM_H
#define INCLUDE_BBS_HAL_TERM_H

#include "bbs/types.h"
#include "bbs/session.h"

/**
 * term_detect_backspace()
 *
 * Present graphics selection menu; set all terminal fields from user choice.
 * No follow-up prompts — width and caps are fixed per mode.
 *
 * Returns:
 *   Detected terminal mode
 */
term_mode_t term_detect_backspace(session_t *s);

/**
 * term_detect_display_file()
 *
 * Display the login welcome file after terminal detection.
 * Uses session_display_file() fallback chain:
 *   G.LOGIN <mode> <width> → G.LOGIN <mode> → G.LOGIN <width> → G.LOGIN
 */
void term_detect_display_file(const session_t *s);

/**
 * term_detect_all()
 *
 * Run complete terminal detection sequence:
 *   1. Graphics menu — sets mode, width, and caps
 *   2. Display login welcome file
 *
 * For local console: skip prompts, force PETSCII/40-column.
 */
void term_detect_all(session_t *s);

#endif /* INCLUDE_BBS_HAL_TERM_H */
