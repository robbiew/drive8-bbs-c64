/* Host unit tests for the pure door-visibility predicate. */
#include "host.h"
#include "bbs/records.h"
#include "bbs/doors.h"

static door_record_t mk(u8 flags, u8 min_level) {
  door_record_t d; memset(&d, 0, sizeof(d));
  d.id = 1; d.flags = flags; d.min_level = min_level; d.cmd_key = 'A';
  return d;
}

int main(void) {
  door_record_t on  = mk(DOOR_F_ENABLED, 2);
  door_record_t off = mk(0, 0);
  EXPECT_EQ("enabled_level_ok",  door_visible(&on, 5), TRUE);
  EXPECT_EQ("enabled_level_low", door_visible(&on, 1), FALSE);
  EXPECT_EQ("disabled",          door_visible(&off, 5), FALSE);
  return test_summary("door_visible");
}
