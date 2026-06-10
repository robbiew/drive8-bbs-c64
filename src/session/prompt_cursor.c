/* Animated color-cycling block cursor at the menu prompt (I/O wiring). */
#include "bbs/prompt_cursor.h"
#include "bbs/session.h"
#include "bbs/cfg.h"
#include "bbs/hal/clock.h"

static bool_t s_armed      = FALSE;
static u8     s_color_idx  = 0;
static u8     s_last_tenth = 0xFF;

/* Cursor only animates for a remote caller, when enabled, on a color mode. */
static bool_t cursor_supported(const session_t *s)
{
  if (!bbs_cfg.prompt_cursor) return FALSE;
  if (s->is_local) return FALSE;
  return (s->term_mode == TERM_PETSCII || s->term_mode == TERM_ANSI_CP437);
}

static void cursor_draw(const session_t *s)
{
  char frame[16];
  if (prompt_cursor_build_frame(s->term_mode, s_color_idx, frame) > 0)
    sess_tx(frame);
}

void prompt_cursor_arm(const session_t *s)
{
  clock_tod_t now;
  if (!cursor_supported(s)) { s_armed = FALSE; return; }
  s_color_idx = 0;
  clock_read(&now);
  s_last_tenth = now.tenths;
  s_armed = TRUE;
  cursor_draw(s);
}

void prompt_cursor_tick(const session_t *s)
{
  clock_tod_t now;
  if (!s_armed) return;
  clock_read(&now);
  if (now.tenths == s_last_tenth) return;   /* throttle to TOD tenths */
  s_last_tenth = now.tenths;
  s_color_idx = (u8)((s_color_idx + 1) % PROMPT_CURSOR_PALETTE_LEN);
  cursor_draw(s);
}

void prompt_cursor_clear(const session_t *s)
{
  if (!s_armed) return;
  s_armed = FALSE;
  sess_reset_color(s);   /* leave color/reverse clean; echoed key overwrites block */
}
