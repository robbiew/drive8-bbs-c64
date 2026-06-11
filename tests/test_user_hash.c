/* Host unit tests for the shared password hash. */
#include "host.h"
#include "bbs/users.h"

int main(void) {
    char a[5] = {0}, b[5] = {0};
    u8 i;

    user_hash_password(1, "PASS", a);
    user_hash_password(1, "PASS", b);
    EXPECT_MEM("deterministic", a, b, 4);

    /* printable 0x21..0x7E: REL-safe (no 0x00) and PETSCII-round-trip-safe */
    user_hash_password(7, "HUNTER2", a);
    for (i = 0; i < 4; i++) {
        EXPECT_EQ("printable.lo", (a[i] >= 0x21), 1);
        EXPECT_EQ("printable.hi", (a[i] <= 0x7E), 1);
    }

    user_hash_password(1, "PASS", a);
    user_hash_password(1, "PAST", b);
    EXPECT_EQ("pw_sensitive", memcmp(a, b, 4) != 0, 1);

    /* same password, different user id -> different hash (salt effective) */
    user_hash_password(1, "PASS", a);
    user_hash_password(2, "PASS", b);
    EXPECT_EQ("id_salt", memcmp(a, b, 4) != 0, 1);

    /* trailing chars up to USER_PASSWORD_MAX (11) affect the result —
     * inputs differ only at index 10, the last storable position */
    user_hash_password(1, "ABCDEFGHIJA", a);
    user_hash_password(1, "ABCDEFGHIJB", b);
    EXPECT_EQ("tail_sensitive", memcmp(a, b, 4) != 0, 1);

    return test_summary("user_hash");
}
