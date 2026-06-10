/* Host unit test for prompt_cursor_build_frame. Compile with gcc:
 *   gcc -Iinclude -o /tmp/tpc tests/test_prompt_cursor_frame.c \
 *       src/session/prompt_cursor_frame.c && /tmp/tpc
 */
#include "bbs/prompt_cursor.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;

static void expect_bytes(const char *name, const char *got, u8 gotlen,
                         const char *want, u8 wantlen)
{
  if (gotlen != wantlen || memcmp(got, want, wantlen) != 0 || got[gotlen] != '\0') {
    printf("FAIL %s: len=%u want=%u\n", name, (unsigned)gotlen, (unsigned)wantlen);
    fails++;
  } else {
    printf("ok   %s\n", name);
  }
}

int main(void)
{
  char buf[16];
  u8 n;

  /* PETSCII frame, color 0 (cyan 0x9f): RVS-on, space, RVS-off, cursor-left. */
  n = prompt_cursor_build_frame(TERM_PETSCII, 0, buf);
  expect_bytes("petscii.0", buf, n, "\x9f\x12\x20\x92\x9d", 5);

  /* PETSCII frame, color 1 (lt green 0x99). */
  n = prompt_cursor_build_frame(TERM_PETSCII, 1, buf);
  expect_bytes("petscii.1", buf, n, "\x99\x12\x20\x92\x9d", 5);

  /* Color index wraps modulo palette length (5 -> 0). */
  n = prompt_cursor_build_frame(TERM_PETSCII, 5, buf);
  expect_bytes("petscii.wrap", buf, n, "\x9f\x12\x20\x92\x9d", 5);

  /* ANSI frame, color 0 (bg 46): ESC[46m, space, ESC[D. */
  n = prompt_cursor_build_frame(TERM_ANSI_CP437, 0, buf);
  expect_bytes("ansi.0", buf, n, "\x1b[46m \x1b[D", 9);

  /* ANSI frame, color 3 (bg 41). */
  n = prompt_cursor_build_frame(TERM_ANSI_CP437, 3, buf);
  expect_bytes("ansi.3", buf, n, "\x1b[41m \x1b[D", 9);

  /* ASCII: no frame. */
  n = prompt_cursor_build_frame(TERM_ASCII, 0, buf);
  expect_bytes("ascii.none", buf, n, "", 0);

  printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
  return fails ? 1 : 0;
}
