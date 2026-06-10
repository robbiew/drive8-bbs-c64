/* Pure builder for the prompt cursor's per-frame byte sequence. No I/O. */
#include "bbs/prompt_cursor.h"

/* Vivid, evenly-spaced colors: cyan, lt green, yellow, lt red, lt blue. */
static const u8 s_petscii_colors[PROMPT_CURSOR_PALETTE_LEN] = {
  0x9f, 0x99, 0x9e, 0x96, 0x9a
};
/* Matching ANSI background-color digits (SGR 46,42,43,41,44). */
static const char s_ansi_bg[PROMPT_CURSOR_PALETTE_LEN] = {
  '6', '2', '3', '1', '4'
};

u8 prompt_cursor_build_frame(term_mode_t mode, u8 color_idx, char *out)
{
  u8 i = (u8)(color_idx % PROMPT_CURSOR_PALETTE_LEN);

  if (mode == TERM_PETSCII) {
    out[0] = (char)s_petscii_colors[i];
    out[1] = (char)0x12;  /* RVS on */
    out[2] = (char)0x20;  /* space -> solid block in current color */
    out[3] = (char)0x92;  /* RVS off */
    out[4] = (char)0x9d;  /* cursor left -> sit back on the block */
    out[5] = '\0';
    return 5;
  }

  if (mode == TERM_ANSI_CP437) {
    out[0] = (char)0x1b; out[1] = '['; out[2] = '4'; out[3] = s_ansi_bg[i];
    out[4] = 'm';                 /* set background color */
    out[5] = ' ';                 /* space -> solid block */
    out[6] = (char)0x1b; out[7] = '['; out[8] = 'D';  /* cursor left */
    out[9] = '\0';
    return 9;
  }

  out[0] = '\0';
  return 0;
}
