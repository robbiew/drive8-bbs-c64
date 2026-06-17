/* Pure door-visibility predicate — no HAL dependency. */
#include "bbs/doors.h"

bool_t door_visible(const door_record_t *rec, u8 level) {
  if (!rec || rec->id == 0) return FALSE;
  if (!(rec->flags & DOOR_F_ENABLED)) return FALSE;
  if (level < rec->min_level) return FALSE;
  return TRUE;
}
