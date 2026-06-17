/* Door plugin cross-build ABI — shared by BOOT and the dev kit. APPEND-ONLY. */
#ifndef INCLUDE_BBS_DOOR_ABI_H
#define INCLUDE_BBS_DOOR_ABI_H

#include "bbs/types.h"

#define BBS_DCB_ADDR     0x033Cu   /* cassette buffer: fixed handshake slot */
#define BBS_DCB_MAGIC0   0          /* +0: 'D' */
#define BBS_DCB_MAGIC1   1          /* +1: '6' */
#define BBS_DCB_VER      2          /* +2: abi version */
#define BBS_DCB_PTR_LO   3          /* +3..+4: api struct pointer */
#define BBS_DCB_PTR_HI   4

#define BBS_DOOR_MAGIC0  'D'
#define BBS_DOOR_MAGIC1  '6'
#define BBS_ABI_VERSION  1

#define BBS_DOOR_HDR_MAGIC 3        /* door PRG: magic at $9700+3 */
#define BBS_DOOR_HDR_VER   5        /* door PRG: abi byte at $9700+5 */

typedef struct {
  char handle[16];
  u8   access_level;
  char firstname[16];
  char lastname[16];
  char location[21];
  u8   term_width;
  u8   term_mode;
} bbs_caller_t;

typedef struct {
  u8   version;
  void (*print)(const char *cp437);
  void (*print_n)(const char *buf, u8 len);
  void (*display_file)(u8 cat, const char *name);
  void (*clear_screen)(void);
  u8   (*getkey)(void);
  i8   (*read_line)(char *buf, u8 max);
  void (*get_caller)(bbs_caller_t *out);
} bbs_api_t;

static inline bool_t door_abi_check(u8 magic0, u8 magic1, u8 ver) {
  return (magic0 == BBS_DOOR_MAGIC0 && magic1 == BBS_DOOR_MAGIC1
          && ver == BBS_ABI_VERSION) ? TRUE : FALSE;
}

#endif
