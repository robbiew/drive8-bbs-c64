/**
 * TURBO/64 BBS — Menu and Prompt System
 *
 * Hierarchical menu framework supporting v1 (static, hardcoded) and
 * v1.1+ (disk-based, admin-configurable) menu structures.
 */

#ifndef INCLUDE_BBS_MENU_H
#define INCLUDE_BBS_MENU_H

#include "types.h"
#include "err.h"

/* Forward declaration - session_t is defined in session.h */
struct session_s;
typedef struct session_s session_t;

/**
 * Menu command entry descriptor
 *
 * Each command occupies one character slot and can either invoke
 * an action handler or transition to a submenu.
 */
typedef struct {
  char   cmd;             /* Single-char command ('M', 'E', 'Q', etc.) */
  char   label[16];       /* Display name ("MESSAGE AREAS", "EMAIL", etc.) — 15 chars max */
  char   submenu_name[8]; /* Submenu ID to enter (empty if is_action=TRUE) — 7 chars max */
  u8     access_level;    /* Min access level required (v1: 0 = all users) */
  bool_t is_action;       /* TRUE = invoke handler; FALSE = enter submenu */
  void   (*handler)(session_t *s); /* Action handler function (if is_action=TRUE) */
} menu_cmd_t;

/**
 * Menu definition
 *
 * v1: static array lookup, compiled in.
 * v1.1+: loaded from disk (REL file "MENUS").
 */
typedef struct {
  char          id[8];         /* Unique menu ID ('main', 'msgs', 'email', etc.) — 7 chars max */
  char          title[16];     /* Display title ("MAIN MENU", "MESSAGE AREAS", etc.) — 15 chars max */
  char          file_base[8];  /* Base name for m.* (menu) and p.* (prompt) files — 7 chars max */
  menu_cmd_t    *commands;     /* Pointer to command array */
  u8            command_count; /* Number of commands in array */
} menu_def_t;

/**
 * menu_init()
 *
 * Initialize the menu system at startup.
 * - Builds static menu and command tables (v1)
 * - Verifies all prompt gfiles exist on disk
 * - Called once during boot_sequence()
 *
 * Returns:
 *   BBS_OK     — menu system ready
 *   BBS_EIO    — gfile access error
 *   BBS_EFATAL — menu table corrupted
 */
bbs_err_t menu_init(void);

/**
 * menu_find()
 *
 * Look up a menu definition by ID.
 *
 * Parameters:
 *   menu_id - menu identifier (e.g., "main", "msgs")
 *
 * Returns:
 *   Pointer to menu_def_t on success
 *   NULL if menu not found
 */
menu_def_t *menu_find(const char *menu_id);

/**
 * menu_enter()
 *
 * Transition to a new menu, pushing current menu to stack.
 * - Saves current menu_state.current_menu to parent_menus stack
 * - Increments depth
 * - Clears menu_displayed flag (forces re-render next loop iteration)
 * - Caps depth at 5 levels; deeper calls are ignored
 *
 * Parameters:
 *   s       - session context
 *   menu_id - target menu ID
 */
void menu_enter(session_t *s, const char *menu_id);

/**
 * menu_back()
 *
 * Pop to parent menu or request logoff if at depth 0.
 * - If depth > 0: pops parent_menus stack, decrements depth
 * - If depth == 0: sets session state to SESS_LOGOFF (quit BBS)
 * - Always clears menu_displayed flag (forces re-render)
 *
 * Parameters:
 *   s - session context
 */
void menu_back(session_t *s);

/**
 * menu_dispatch()
 *
 * Process a single menu command character.
 *
 * - Looks up 'ch' in current menu's command table
 * - Special cases:
 *   'Q': calls menu_back()
 *   '?': calls menu_display() (re-render)
 *   Unknown: sends "UNKNOWN COMMAND\r\n" message, does not re-display
 * - If command found:
 *   - If is_action=TRUE: invokes handler(s)
 *   - If is_action=FALSE: calls menu_enter(s, submenu_name)
 * - After action/transition, does NOT re-display;
 *   session loop will call menu_display() on next iteration
 *
 * Parameters:
 *   s  - session context
 *   ch - command character (already uppercased by caller)
 */
void menu_dispatch(session_t *s, char ch);

/**
 * menu_display()
 *
 * Render the current menu to the session.
 *
 * Flow:
 * 1. Look up current menu by ID (via menu_state.current_menu)
 * 2. Send title string with CRLF
 * 3. Load and display prompt gfile (via session_display_file with term translation)
 * 4. List all available commands with labels
 * 5. Filter by access level (v1: show all; v1.1+: hide unavailable)
 * 6. Send prompt for input (e.g., "\r\nCOMMAND: ")
 *
 * Parameters:
 *   s - session context
 *
 * Returns:
 *   BBS_OK      — menu displayed successfully
 *   BBS_ENOTFOUND — current menu not found (fatal, sets SESS_ERROR state)
 */
bbs_err_t menu_display(session_t *s);

#endif /* INCLUDE_BBS_MENU_H */
