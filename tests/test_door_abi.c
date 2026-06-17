/* Host unit tests for the door ABI validator + layout constants. */
#include "host.h"
#include "bbs/door_abi.h"

int main(void) {
  EXPECT_EQ("dcb_addr",   BBS_DCB_ADDR, 0x033C);
  EXPECT_EQ("abi_ver",    BBS_ABI_VERSION, 1);
  EXPECT_EQ("hdr_magic_off", BBS_DOOR_HDR_MAGIC, 3);
  EXPECT_EQ("hdr_ver_off",   BBS_DOOR_HDR_VER, 5);

  EXPECT_EQ("good",      door_abi_check('D','6', BBS_ABI_VERSION), TRUE);
  EXPECT_EQ("bad_magic", door_abi_check('X','6', BBS_ABI_VERSION), FALSE);
  EXPECT_EQ("bad_ver",   door_abi_check('D','6', BBS_ABI_VERSION + 1), FALSE);
  return test_summary("door_abi");
}
