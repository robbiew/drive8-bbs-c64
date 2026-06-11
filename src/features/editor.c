/* TURBO/64 BBS — Line editor (REU compose buffer). */
#include "bbs/editor.h"
#include "bbs/hal/reu.h"
#include "bbs/cfg.h"
#include "bbs/session.h"
#include "bbs/config.h"
#include <string.h>
#include <stdio.h>
#include "bbs/overlay.h"

/* editor_crossline_bs — lives in the main code section so its locals do NOT
 * consume msgs_bss frame space.  Pops the last committed line from the
 * compose buffer, removes its final character (cross-line backspace), loads
 * the result into linebuf, and returns the new length. */
static u8 editor_crossline_bs(char *linebuf, u8 *line_count)
{
  u16 buf_len = reu_compose_len();
  u16 ls = 0;   /* compose offset where the last line starts */
  u8 ll;
  if (buf_len >= 2u) {
    u16 pos = buf_len - 2u;
    char c;
    for (;;) {
      reu_compose_read(pos, &c, 1);
      if (c == '\n') { ls = (u16)(pos + 1u); break; }
      if (pos == 0u) break;
      pos--;
    }
  }
  ll = (buf_len > 0u) ? (u8)((buf_len - 1u) - ls) : 0u;
  if (ll > 37u) ll = 37u;
  if (ll > 0u) { ll--; reu_compose_read(ls, linebuf, (u16)ll); }
  linebuf[ll] = '\0';
  reu_compose_truncate(ls);
  (*line_count)--;
  return ll;
}

/* editor_count_lines — count committed lines in the compose buffer by
 * scanning for newline terminators.  Used when resuming the editor with
 * pre-populated content (e.g. an injected quote) so the line limit and
 * cross-line backspace stay in sync.  Lives in the main code section so its
 * locals do NOT consume msgs_bss frame space. */
static u8 editor_count_lines(void)
{
  u16 len = reu_compose_len();
  u16 pos = 0;
  u8  n = 0;
  char c;
  while (pos < len) {
    reu_compose_read(pos, &c, 1);
    if (c == '\n' && n < 0xFFu) n++;
    pos++;
  }
  return n;
}

#pragma code(msgs_code)
#pragma data(msgs_data)
#pragma bss(msgs_bss)

/* s_line_start removed — was written by never read (dead BSS). */
static u8   s_line_count;

editor_result_t editor_run(const session_t *s, const char *context_line)
{
  char s_linebuf[41];
  u8 linelen = 0;
  u8 ch;
  u16 max_chars = bbs_cfg.reu_enabled ? CFG_EDITOR_MAX_CHARS : CFG_COMPOSE_BUF_FALLBACK;

  if (reu_compose_len()) { s_line_count = editor_count_lines(); }
  else { reu_compose_init(); s_line_count = 0; }
  memset(s_linebuf, 0, sizeof(s_linebuf));

  if (context_line) {
    sess_tx("\r\n");
    sess_tx(context_line); sess_tx("\r\n");
    sess_tx("/S=SAVE /A=ABORT\r\n: ");
  } else {
    sess_tx(": ");
  }

  for (;;) {
    if (!sess_getc(&ch)) {
      if (!sess_carrier_ok(s)) return EDITOR_ABORT;
      continue;
    }

    if (ch == 8 || ch == 20) {
      if (linelen > 0) {
        linelen--;
        s_linebuf[linelen] = '\0';
        sess_erase_char(s);
      } else if (s_line_count > 0) {
        linelen = editor_crossline_bs(s_linebuf, &s_line_count);
        sess_tx("\r\n: "); sess_tx(s_linebuf);
      }
      continue;
    }

    if (ch == 13 || ch == '\n') {
      s_linebuf[linelen] = '\0';
      if (linelen >= 2 && s_linebuf[0] == '/') {
        char cmd = (s_linebuf[1] >= 'a' && s_linebuf[1] <= 'z') ? (char)(s_linebuf[1]-32) : s_linebuf[1];
        u8 k;
        /* Erase the command line (": " prompt + typed chars) so it leaves no trace. */
        for (k = 0; k < (u8)(linelen + 2); k++) sess_erase_char(s);
        if (cmd == 'S') return EDITOR_SAVE;
        if (cmd == 'Q') return EDITOR_QUOTE;
        if (cmd == 'A') {
          u8 c2;
          sess_tx("ABORT? (Y/N) ");
          while (!sess_getc(&c2)) {}   /* brief hang on disconnect acceptable here */
          sess_tx("\r\n");
          c2 = (c2 >= 'a' && c2 <= 'z') ? (u8)(c2-32) : c2;
          if (c2 == 'Y') { reu_compose_init(); s_line_count = 0; return EDITOR_ABORT; }
          sess_tx("CONTINUING.\r\n");
        }
        linelen = 0; memset(s_linebuf, 0, sizeof(s_linebuf));
        sess_tx(": "); continue;
      }
      sess_tx("\r\n");
      if (s_line_count < CFG_EDITOR_MAX_LINES &&
          linelen > 0 && reu_compose_len() + linelen + 1 < max_chars) {
        s_line_count++;
        reu_compose_puts(s_linebuf);
        reu_compose_putc('\n');
      } else if (s_line_count >= CFG_EDITOR_MAX_LINES) {
        sess_tx("LINE LIMIT. /S TO SAVE.\r\n");
      }
      linelen = 0; memset(s_linebuf, 0, sizeof(s_linebuf));
      sess_tx(": "); continue;
    }

    if (ch >= 0x20 && ch < 0x7f && linelen < 37) {
      char e[2]; s_linebuf[linelen++] = (char)ch; s_linebuf[linelen] = '\0';
      e[0] = (char)ch; e[1] = '\0'; sess_tx(e);
      /* Line full — auto-commit and start a new prompt line.
       * Limit is 37: 40-col display minus 2-char ": " prompt, minus 1
       * to keep the rightmost column visually empty. */
      if (linelen == 37) {
        sess_tx("\r\n");
        if (s_line_count < CFG_EDITOR_MAX_LINES &&
            reu_compose_len() + 37 + 1 < max_chars) {
          s_line_count++;
          reu_compose_puts(s_linebuf);
          reu_compose_putc('\n');
        } else if (s_line_count >= CFG_EDITOR_MAX_LINES) {
          sess_tx("LINE LIMIT. /S TO SAVE.\r\n");
        }
        linelen = 0; memset(s_linebuf, 0, sizeof(s_linebuf));
        sess_tx(": ");
      }
    }
  }
}
#pragma code(code)
#pragma data(data)
#pragma bss(bss)
