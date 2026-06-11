/* Shared password hash — pure (no HAL deps) so it compiles host-side for tests. */
#include "bbs/users.h"

/* Iterated, id-salted xorshift32 mix folded to 4 printable bytes.
 * Replaces the reversible 4-byte XOR fold: collisions are no longer
 * constructible by hand and a stolen USR LOG no longer yields password
 * candidates by inspection.  Multiply-free on purpose — FNV-style 32-bit
 * multiplies drag oscar64's long-multiply runtime into a main region that
 * has no room for it.  Output stays in 0x21..0x7E — a stored hash must
 * never contain 0x00 (REL end-of-record) or a control byte (does not
 * survive a PETSCII round-trip; see records.h).  64 rounds keeps a login
 * check well under a second at 1 MHz while multiplying brute-force cost. */
#define HASH_ROUNDS 64

static u32 mix32(u32 h) {
  h ^= h << 13;
  h ^= h >> 17;
  h ^= h << 5;
  return h;
}

void user_hash_password(u8 user_id, const char *password, char *out_hash) {
  u32 h = 2166136261uL ^ user_id;
  u8 r, i;

  for (r = 0; r < HASH_ROUNDS; r++) {
    h = mix32(h ^ r);
    for (i = 0; i < USER_PASSWORD_MAX && password[i]; i++) {
      h = mix32(h ^ (u8)password[i]);
    }
  }

  /* % 0x5E has ~1.5% modulo bias — negligible at this hash size. */
  for (i = 0; i < 4; i++) {
    out_hash[i] = (char)(0x21 + ((u8)(h >> (i * 8)) % 0x5E));
  }
}
