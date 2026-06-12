/**
 * TURBO/64 BBS — Menu System Implementation
 *
 * Core menu functions: initialization, navigation, dispatch, display.
 * v1: static compile-time menus; v1.1+: disk-based menus.
 */

#include <string.h>
#include <stdio.h>

#include "bbs/menu.h"
#include "bbs/session.h"
#include "bbs/types.h"
#include "bbs/err.h"

/* Forward declarations (menu_tables.c) */
extern menu_def_t menus[];
extern u8 menu_count;

/**
 * menu_init()
 *
 * Initialize the menu system at startup.
 * v1: Verifies static menu table is valid.
 * v1.1+: Will load menus from disk.
 */
bbs_err_t menu_init(void) {
  /* v1: Static table validation */
  if (menu_count == 0) {
    return BBS_EFATAL;
  }

  /* v1: Basic sanity check on MAIN menu */
  const menu_def_t *main_menu = menu_find("main");
  if (!main_menu) {
    return BBS_EFATAL;
  }

  /* TODO: v1.1+ - Load menus from MENUS REL file, verify gfiles exist */

  return BBS_OK;
}

/**
 * menu_find()
 *
 * Look up a menu definition by ID.
 */
menu_def_t *menu_find(const char *menu_id) {
  u8 i;

  if (!menu_id || !menu_id[0]) {
    return NULL;
  }

  /* v1: Linear search through static menu table */
  for (i = 0; i < menu_count; i++) {
    if (strcmp(menus[i].id, menu_id) == 0) {
      return &menus[i];
    }
  }

  return NULL;
}

/**
 * menu_enter()
 *
 * Transition to a new menu, pushing current menu to stack.
 */
void menu_enter(session_t *s, const char *menu_id) {
  const menu_def_t *target;

  if (!s || !menu_id) {
    return;
  }

  /* Verify target menu exists */
  target = menu_find(menu_id);
  if (!target) {
    return;
  }

  /* Cap depth at 5 levels */
  if (s->menu_state.depth >= 5) {
    return;
  }

  /* Save current menu to parent stack */
  if (s->menu_state.depth > 0 || s->menu_state.current_menu[0]) {
    strncpy(s->menu_state.parent_menus[s->menu_state.depth],
            s->menu_state.current_menu, 15);
    s->menu_state.parent_menus[s->menu_state.depth][15] = '\0';
  }

  /* Transition to new menu */
  strncpy(s->menu_state.current_menu, menu_id, 15);
  s->menu_state.current_menu[15] = '\0';
  s->menu_state.depth++;

  /* Force re-render */
  s->menu_displayed = FALSE;
}

/**
 * menu_back()
 *
 * Pop to parent menu or request logoff if at depth 0.
 */
void menu_back(session_t *s) {
  if (!s) {
    return;
  }

  if (s->menu_state.depth == 0) {
    /* At MAIN level: logoff */
    s->state = SESS_LOGOFF;
  } else {
    /* Pop parent menu from stack */
    s->menu_state.depth--;
    strncpy(s->menu_state.current_menu,
            s->menu_state.parent_menus[s->menu_state.depth], 15);
    s->menu_state.current_menu[15] = '\0';
  }

  /* Force re-render */
  s->menu_displayed = FALSE;
}

/**
 * menu_dispatch()
 *
 * Process a single menu command character.
 */
void menu_dispatch(session_t *s, char ch) {
  menu_def_t *menu;
  // cppcheck-suppress variableScope
  const menu_cmd_t *cmd;
  u8 i;

  if (!s) {
    return;
  }

  /* Look up current menu */
  menu = menu_find(s->menu_state.current_menu);
  if (!menu) {
    return;
  }

  /* Special case: 'Q' always backs or logs off */
  if (ch == 'Q') {
    menu_back(s);
    return;
  }

  /* Special case: '?' re-displays menu */
  if (ch == '?') {
    s->menu_displayed = FALSE;
    return;
  }

  /* Search command table */
  for (i = 0; i < menu->command_count; i++) {
    cmd = &menu->commands[i];

    if (cmd->cmd == ch) {
      /* Access level check (v1: all 0, so always pass; v1.1+: enforce) */
      if (cmd->access_level > s->user.access_level) {
        session_emit(s, "\r\nINSUFFICIENT ACCESS LEVEL\r\n");
        return;
      }

      /* Execute action or enter submenu */
      if (cmd->is_action) {
        /* Invoke action handler if present */
        s->menu_skip_pause = FALSE;
        if (cmd->handler) {
          cmd->handler(s);
        }
        /* If handler changed state to SESS_LOGOFF, don't re-display.
         * Full-screen handlers (e.g. message areas) set menu_skip_pause to
         * skip the [PRESS ANY KEY] pause and return straight to the menu. */
        if (s->state != SESS_LOGOFF) {
          s->menu_displayed = FALSE;
          s->menu_needs_pause = !s->menu_skip_pause;
        }
      } else {
        /* Enter submenu */
        menu_enter(s, cmd->submenu_name);
      }
      return;
    }
  }

  /* Command not found.  Remote sessions get a hard cap: if NO CARRIER is ever
   * missed (the only hangup signal under VICE/tcpser), the far modem sits in
   * command mode echoing our replies back as input — an endless loop that also
   * defeats the idle watchdog, since echoed bytes count as activity.  The cap
   * is total, not consecutive: the reflected reply text contains valid command
   * letters that would keep resetting a consecutive counter. */
  if (!s->is_local && ++s->menu_state.unknown_count >= MENU_GARBAGE_LIMIT) {
    session_emit(s, "\r\nLINE NOISE - DROPPING CARRIER\r\n");
    s->state = SESS_LOGOFF;
    return;
  }
  session_emit(s, "\r\nUNKNOWN COMMAND. [?] FOR HELP\r\n");
}

/**
 * menu_display()
 *
 * Render the current menu to the session.
 * If the menu gfile doesn't exist, generates a system ASCII fallback.
 */
bbs_err_t menu_display(session_t *s) {
  menu_def_t *menu;
  // cppcheck-suppress variableScope
  u8 i;
  // cppcheck-suppress variableScope
  char line_buf[80];
  bbs_err_t gfile_err;

  if (!s) {
    return BBS_EBADARG;
  }

  /* Look up current menu */
  menu = menu_find(s->menu_state.current_menu);
  if (!menu) {
    s->state = SESS_ERROR;
    s->error = BBS_ENOTFOUND;
    return BBS_ENOTFOUND;
  }

  /* Clear screen before rendering menu */
  session_clear_screen(s);

  /* Try to display menu file (m.*) — terminal-aware fallback chain.
   * If found, the file IS the menu — skip system-generated command list.
   * If not found, emit a system-generated list as fallback. */
  gfile_err = session_display_file(s, 'm', menu->file_base);

  if (gfile_err == BBS_ENOTFOUND) {
    /* No menu file: emit system-generated title + command list */
    sess_reset_color(s);
    session_emit(s, "\r\n");
    session_emit(s, menu->title);
    session_emit(s, "\r\n");
    session_emit(s, "======================================\r\n");
    session_emit(s, "\r\n");
    for (i = 0; i < menu->command_count; i++) {
      const menu_cmd_t *cmd = &menu->commands[i];
      if (cmd->access_level > s->user.access_level) continue;
      sprintf(line_buf, " [%c] %s\r\n", cmd->cmd, cmd->label);
      session_emit(s, line_buf);
    }
  }

  /* Optional prompt file (p.*) — if present, replaces system "COMMAND: " prompt */
  if (session_display_file(s, 'p', menu->file_base) == BBS_ENOTFOUND) {
    sess_reset_color(s);
    session_emit(s, "\r\nCOMMAND: ");
  }

  return BBS_OK;
}
