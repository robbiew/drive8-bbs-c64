/**
 * TURBO/64 BBS — Static Menu & Command Tables (v1)
 *
 * Hardcoded menu and command definitions for TURBO/64 v0.1.0.
 * v1.1+: These will be migrated to disk-based REL records.
 *
 * Action handlers are forward-declared as extern and defined in menu_actions.c.
 */

#include "bbs/menu.h"
#include "bbs/session.h"

/* Forward declarations of action handlers (menu_actions.c) */
/* MAIN menu actions */
extern void action_last_callers(session_t *s);
extern void action_user_list(session_t *s);
extern void action_system_info(session_t *s);
extern void action_help(session_t *s);

/* MSGS menu actions */
extern void action_list_boards(session_t *s);

/* EMAIL menu actions */
extern void action_list_mail(session_t *s);
extern void action_send_mail(session_t *s);

/* PREFS menu actions */
extern void action_term_mode(session_t *s);
extern void action_term_width(session_t *s);
extern void action_colors(session_t *s);
extern void action_paging(session_t *s);
extern void action_clear_on_msg(session_t *s);

/* FILES menu actions */
extern void action_list_areas(session_t *s);
extern void action_download_file(session_t *s);
extern void action_upload_file(session_t *s);
extern void action_search_files(session_t *s);

/* DOOR menu actions */
extern void action_run_door(session_t *s);

/**
 * MAIN Menu Commands
 *
 * v1: Gateway to 6 submenus + utility displays
 */
static menu_cmd_t main_commands[] = {
  { 'M', "MESSAGE AREAS",     "",      0, TRUE,  action_list_boards },
  { 'E', "EMAIL",             "email", 0, FALSE, NULL },
  { 'P', "PREFERENCES",       "prefs", 0, FALSE, NULL },
  { 'F', "FILES",             "files", 0, FALSE, NULL },
  { 'D', "DOORS",             "door",  0, FALSE, NULL },
  { 'L', "LAST CALLERS",      "",      0, TRUE,  action_last_callers },
  { 'U', "USERS ONLINE",      "",      0, TRUE,  action_user_list },
  { 'I', "SYSTEM INFO",       "",      0, TRUE,  action_system_info },
  { '?', "HELP",              "",      0, TRUE,  action_help },
  { 'Q', "GOODBYE",           "",      0, TRUE,  NULL }, /* Special: menu_back() */
};


/**
 * EMAIL Menu Commands
 */
static menu_cmd_t email_commands[] = {
  { 'L', "LIST MAIL",         "",      0, TRUE,  action_list_mail },
  { 'S', "SEND MAIL",         "",      0, TRUE,  action_send_mail },
  { '?', "HELP",              "",      0, TRUE,  action_help },
  { 'Q', "BACK TO MAIN",      "",      0, TRUE,  NULL },
};

/**
 * PREFS (Preferences) Menu Commands
 */
static menu_cmd_t prefs_commands[] = {
  { 'T', "TERMINAL MODE",     "",      0, TRUE,  action_term_mode },
  { 'W', "TERMINAL WIDTH",    "",      0, TRUE,  action_term_width },
  { 'C', "COLORS",            "",      0, TRUE,  action_colors },
  { 'P', "PAGING",            "",      0, TRUE,  action_paging },
  { 'S', "SCREEN CLEAR", "", 0, TRUE, action_clear_on_msg },
  { '?', "HELP",              "",      0, TRUE,  action_help },
  { 'Q', "BACK TO MAIN",      "",      0, TRUE,  NULL },
};

/**
 * FILES Menu Commands
 */
static menu_cmd_t files_commands[] = {
  { 'L', "LIST AREAS",        "",      0, TRUE,  action_list_areas },
  { 'D', "DOWNLOAD FILE",     "",      0, TRUE,  action_download_file },
  { 'U', "UPLOAD FILE",       "",      0, TRUE,  action_upload_file },
  { 'S', "SEARCH FILES",      "",      0, TRUE,  action_search_files },
  { '?', "HELP",              "",      0, TRUE,  action_help },
  { 'Q', "BACK TO MAIN",      "",      0, TRUE,  NULL },
};

/**
 * DOOR (Games & External Programs) Menu Commands
 */
static menu_cmd_t door_commands[] = {
  { 'R', "RUN DOOR",          "",      0, TRUE,  action_run_door },
  { '?', "HELP",              "",      0, TRUE,  action_help },
  { 'Q', "BACK TO MAIN",      "",      0, TRUE,  NULL },
};

/**
 * Master Menu Table
 *
 * All available menus and their associated command tables.
 */
menu_def_t menus[] = {
  { "main",  "MAIN MENU",        "main",  main_commands,  sizeof(main_commands)  / sizeof(main_commands[0])  },
  { "email", "EMAIL",            "email", email_commands, sizeof(email_commands) / sizeof(email_commands[0]) },
  { "prefs", "PREFERENCES",      "prefs", prefs_commands, sizeof(prefs_commands) / sizeof(prefs_commands[0]) },
  { "files", "FILES",            "files", files_commands, sizeof(files_commands) / sizeof(files_commands[0]) },
  { "door",  "DOORS & GAMES",    "door",  door_commands,  sizeof(door_commands)  / sizeof(door_commands[0])  },
};

/**
 * Menu count (for iteration in menu_find(), menu_init(), etc.)
 */
u8 menu_count = sizeof(menus) / sizeof(menus[0]);
