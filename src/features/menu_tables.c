/**
 * TURBO/64 BBS — Static Menu & Command Tables (v1)
 *
 * Hardcoded menu and command definitions for TURBO/64 v0.2.0.
 * v1.1+: These will be migrated to disk-based REL records.
 *
 * Action handlers are forward-declared as extern and defined in menu_actions.c.
 */

#include "bbs/menu.h"
#include "bbs/session.h"
#include "bbs/menu_actions.h"
#include "bbs/doors.h"

/**
 * MAIN Menu Commands
 *
 * v1: Gateway to 6 submenus + utility displays
 */
static menu_cmd_t main_commands[] = {
  { 'M', "MESSAGE AREAS",     "",      0, TRUE,  action_list_boards },
  { 'E', "EMAIL",             "email", 0, FALSE, NULL },
  { 'P', "PREFERENCES",       "prefs", 0, FALSE, NULL },
  { 'F', "FILES",             "",      0, TRUE,  action_files },
  { 'D', "DOORS",             "door",  0, FALSE, NULL },
  { '!', "DOOR PROGRAMS",    "",      0, TRUE,  action_doors },
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
  { "door",  "DOORS & GAMES",    "door",  door_commands,  sizeof(door_commands)  / sizeof(door_commands[0])  },
};

/**
 * Menu count (for iteration in menu_find(), menu_init(), etc.)
 */
u8 menu_count = sizeof(menus) / sizeof(menus[0]);
