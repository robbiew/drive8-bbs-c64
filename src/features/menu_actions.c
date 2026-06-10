/**
 * TURBO/64 BBS — Menu Action Handlers
 *
 * Implementation of menu command action handlers.
 * v1: Mostly stubs and simple info displays.
 * v1.1+: Will integrate with actual feature modules.
 */
#include "bbs/sysop.h"

#include <stdio.h>
#include "bbs/menu.h"
#include "bbs/session.h"
#include "bbs/bulletin.h"
#include "bbs/users.h"
#include "bbs/cfg.h"
#include "bbs/records.h"
#include <c64/kernalio.h>

/**
 * MAIN Menu Actions
 */

void action_last_callers(session_t *s) {
  /* Displayed via WFC sysop screen; remote user view is future work. */
  if (!s) return;
  session_emit(s, "\r\nLAST CALLERS: SEE WFC SCREEN\r\n");
  s->menu_displayed = FALSE;
}

void action_user_list(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nUSERS ONLINE (STUB)\r\n");
  s->menu_displayed = FALSE;
}

void action_system_info(session_t *s) {
  char info_line[64];
  if (!s) return;
  session_emit(s, "\r\nSYSTEM INFO\r\n");
  sprintf(info_line, "  ACCESS LEVEL: %u\r\n", s->user.access_level);
  session_emit(s, info_line);
  sprintf(info_line, "  CALLS: %u\r\n", s->user.calls);
  session_emit(s, info_line);
  sprintf(info_line, "  CREDIT: %u\r\n", s->user.credit_balance);
  session_emit(s, info_line);
  s->menu_displayed = FALSE;
}

void action_help(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nHELP (STUB)\r\n");
  s->menu_displayed = FALSE;
}

/**
 * MSGS (Message Areas) Menu Actions
 */

/* action_list_boards — entry for all MSG menu actions.
 * Loads OVL_MSGS overlay from system disk then runs bulletin_run.  The overlay
 * occupies $9D80-$BFEF; core code at $0880-$9D7F stays resident for callbacks.
 * Reloading on each entry is safe (same bytes, same address) and fast on CMD HD. */
void action_list_boards(session_t *s) {
  krnio_setnam(P"OVL_MSGS");
  krnio_load(1, bbs_cfg.device_system, 1);
  wfc.ovl_wfc_loaded = FALSE;  /* OVL_MSGS displaced OVL_WFC; reload on next wfc call */
  bulletin_run(s);
  wfc_reload();  /* restore OVL_WFC (spy view code) before returning to menu */
  s->menu_displayed = FALSE;
}


/**
 * EMAIL Menu Actions
 */

void action_list_mail(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nLIST MAIL (STUB)\r\n");
  s->menu_displayed = FALSE;
}

void action_send_mail(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nSEND MAIL (STUB)\r\n");
  s->menu_displayed = FALSE;
}

/**
 * PREFS (Preferences) Menu Actions
 */

void action_term_mode(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nTERMINAL MODE\r\n");
  session_emit(s, "  CURRENT: ");
  switch (s->term_mode) {
    case TERM_PETSCII:
      session_emit(s, "PETSCII\r\n");
      break;
    case TERM_ANSI_CP437:
      session_emit(s, "ANSI/CP437\r\n");
      break;
    case TERM_ASCII:
      session_emit(s, "ASCII\r\n");
      break;
    default:
      session_emit(s, "UNKNOWN\r\n");
      break;
  }
  session_emit(s, "  (CHANGE NOT YET IMPLEMENTED)\r\n");
  s->menu_displayed = FALSE;
}

void action_term_width(session_t *s) {
  char info_line[32];
  if (!s) return;
  session_emit(s, "\r\nTERMINAL WIDTH\r\n");
  sprintf(info_line, "  CURRENT: %u COLUMNS\r\n", s->term_width);
  session_emit(s, info_line);
  session_emit(s, "  (CHANGE NOT YET IMPLEMENTED)\r\n");
  s->menu_displayed = FALSE;
}

void action_colors(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nCOLOR SETTINGS (STUB)\r\n");
  s->menu_displayed = FALSE;
}

void action_paging(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nPAGING SETTINGS (STUB)\r\n");
  s->menu_displayed = FALSE;
}

void action_clear_on_msg(session_t *s) {
  if (!s || s->user_id == 0) return;
  s->user.flags ^= USER_F_CLEAR_ON_MSG;
  user_save(&s->user, bbs_cfg.device_system);
  session_emit(s, "\r\nSCREEN CLEAR: ");
  session_emit(s, (s->user.flags & USER_F_CLEAR_ON_MSG) ? "ON\r\n" : "OFF\r\n");
  s->menu_displayed = FALSE;
}

/**
 * FILES Menu Actions
 */

void action_list_areas(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nLIST AREAS (STUB)\r\n");
  s->menu_displayed = FALSE;
}

void action_download_file(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nDOWNLOAD FILE (STUB)\r\n");
  s->menu_displayed = FALSE;
}

void action_upload_file(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nUPLOAD FILE (STUB)\r\n");
  s->menu_displayed = FALSE;
}

void action_search_files(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nSEARCH FILES (STUB)\r\n");
  s->menu_displayed = FALSE;
}

/**
 * DOOR Menu Actions
 */

void action_run_door(session_t *s) {
  if (!s) return;
  session_emit(s, "\r\nRUN DOOR (STUB)\r\n");
  s->menu_displayed = FALSE;
}
