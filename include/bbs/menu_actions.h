/* Menu action handlers (menu_actions.c) — bound into the static menu tables. */
#ifndef INCLUDE_BBS_MENU_ACTIONS_H
#define INCLUDE_BBS_MENU_ACTIONS_H

#include "bbs/session.h"

/* MAIN menu */
void action_last_callers(session_t *s);
void action_user_list(session_t *s);
void action_system_info(session_t *s);
void action_help(session_t *s);

/* MSGS menu */
void action_list_boards(session_t *s);

/* EMAIL menu */
void action_list_mail(session_t *s);
void action_send_mail(session_t *s);

/* PREFS menu */
void action_term_mode(session_t *s);
void action_term_width(session_t *s);
void action_colors(session_t *s);
void action_paging(session_t *s);
void action_clear_on_msg(session_t *s);

/* FILES menu */
void action_list_areas(session_t *s);
void action_download_file(session_t *s);
void action_upload_file(session_t *s);
void action_search_files(session_t *s);

/* DOOR menu */
void action_doors(session_t *s);    /* resident shim: loads OVL_DOORS, calls action_doors_menu */
void action_run_door(session_t *s);

#endif /* INCLUDE_BBS_MENU_ACTIONS_H */
