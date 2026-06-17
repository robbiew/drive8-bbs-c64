/* Door plugin BBS-internal API: data layer + runtime + menu. */
#ifndef INCLUDE_BBS_DOORS_H
#define INCLUDE_BBS_DOORS_H

#include "bbs/types.h"
#include "bbs/records.h"
#include "bbs/session.h"

u8        door_count(u8 device);
bbs_err_t door_by_index(u8 n, door_record_t *out, u8 device);
bbs_err_t door_by_id(u8 id, door_record_t *out, u8 device);
bbs_err_t door_by_key(char key, door_record_t *out, u8 device);
bbs_err_t door_save(const door_record_t *rec, u8 device);
bbs_err_t door_delete(u8 id, u8 device);

bool_t    door_visible(const door_record_t *rec, u8 level);

void      door_run(session_t *s, const door_record_t *rec);
/* CAUTION: action_doors_menu and login_doors_iter live in OVL_DOORS overlay —
 * never call them directly; use the resident shims (action_doors /
 * session_run_login_doors) which load the overlay first. */
void      action_doors_menu(session_t *s);
void      login_doors_iter(session_t *s);
void      session_run_login_doors(session_t *s);

#endif /* INCLUDE_BBS_DOORS_H */
