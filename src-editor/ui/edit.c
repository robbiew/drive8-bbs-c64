/* CONFIGURE UI - Inline field editor for forms. */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ui.h"

/* A field is blank if it has no visible (non-space) character. */
static int is_blank(const char *s)
{
  for (; *s; s++)
    if (*s != ' ') return 0;
  return 1;
}

int ui_read_line(char *buf, int max, u8 case_mode)
{
  int len = 0;
  memset(buf, 0, (unsigned)(max + 1));
  for (;;) {
    char ch = getchar();
    if (ch == '\n' || ch == 13) break;
    if (ch == 20 || ch == 8) {
      if (len > 0) { len--; buf[len] = 0; printf("\x08 \x08"); }
      continue;
    }
    if (len < max && ch >= 32 && ch <= 126) {
      if (case_mode == UI_CASE_UPPER) ch = (char)toupper((unsigned char)ch);
      buf[len++] = ch;
    }
  }
  buf[len] = '\0';
  return len;
}

void ui_edit_field_single(ui_edit_field_t *field)
{
  char buffer[UI_FIELD_MAX_LEN + 1];
  int len, max_edit;
  const char *err;

  max_edit = (field->max_len > UI_FIELD_MAX_LEN) ? UI_FIELD_MAX_LEN : field->max_len;

  for (;;) {
    /*
     * End the prompt with \n so the user's input starts on a FRESH LINE
     * at column 0. C64 CHRIN (screen editor) reads back from the start of
     * the physical line where the cursor sits when RETURN is pressed.
     */
    printf("%d CHARS MAX.\nNEW %s:\n", max_edit, field->label);

    len = ui_read_line(buffer, max_edit, field->case_mode);

    if (is_blank(buffer)) {
      printf("%s CANNOT BE BLANK.\n", field->label);
      continue;
    }

    if (field->validate) {
      err = field->validate(buffer, field->validate_ctx);
      if (err) { printf("%s\n", err); continue; }
    }

    break;
  }

  strncpy(field->value, buffer, (unsigned)field->max_len);
  field->value[field->max_len] = '\0';
  field->current_len = len;
  field->dirty = 1;
}

void ui_select_field(const char *label, char *value, int max_len,
                     const char **options, int num_options)
{
  int i, sel = 0;
  char ch;

  for (i = 0; i < num_options; i++) {
    if (strcmp(value, options[i]) == 0) { sel = i; break; }
  }

  printf("\n%s:\n", label);
  for (i = 0; i < num_options && i < 9; i++) {
    if (i == sel)
      printf(" [%d] %s *\n", i + 1, options[i]);
    else
      printf(" [%d] %s\n",   i + 1, options[i]);
  }
  printf("CHOICE (RETURN=KEEP): ");
  ch = getchar();
  /* drain the trailing RETURN left in CHRIN buffer */
  { char drain; do { drain = getchar(); } while (drain != '\n' && drain != 13); }
  printf("\n");
  if (ch >= '1' && ch < '1' + num_options) {
    sel = ch - '1';
    strncpy(value, options[sel], max_len);
    value[max_len] = '\0';
  }
}

/* Fields are addressed by letter (A..); reserve S=save, Q=back, so cap the
 * field letters below 'Q' (index 16). The largest form (ACCESS LEVELS) has 11. */
#define UI_FORM_MAX_FIELDS 16

int ui_edit_form(const char *title, ui_edit_field_t *fields, int count)
{
  char valid_chars[UI_FORM_MAX_FIELDS + 3];
  int i;
  int shown = (count < UI_FORM_MAX_FIELDS) ? count : UI_FORM_MAX_FIELDS;

  while (1) {
    char input;
    ui_screen_header(title);

    for (i = 0; i < shown; i++) {
      putchar(' ');
      ui_print_hotkey((char)('A' + i));
      textcolor(COLOR_LT_GREEN);
      printf(" %-10s", fields[i].label);
      textcolor(COLOR_WHITE);
      printf(": %s%s\n", fields[i].value, fields[i].dirty ? " *" : "");
    }

    printf("\n ");
    ui_print_hotkey('S'); textcolor(COLOR_LT_GREEN); printf(" SAVE & EXIT   ");
    ui_print_hotkey('Q'); textcolor(COLOR_LT_GREEN); printf(" CANCEL\n");
    textcolor(COLOR_WHITE);
    printf("\n");

    strcpy(valid_chars, "SQ");
    {
      int vlen = 2;
      for (i = 0; i < shown; i++)
        valid_chars[vlen++] = (char)('A' + i);
      valid_chars[vlen] = '\0';
    }

    input = ui_menu_input("CHOICE:", valid_chars);

    if (input == 'S') {
      return 0;
    } else if (input == 'Q') {
      int any_dirty = 0;
      for (i = 0; i < shown; i++)
        if (fields[i].dirty) { any_dirty = 1; break; }
      /* Guard against losing edits to a stray keypress; clean form exits at once. */
      if (any_dirty && !ui_confirm("DISCARD CHANGES?")) continue;
      return -1;
    } else if (input >= 'A' && input < (char)('A' + shown)) {
      ui_edit_field_t *f = &fields[input - 'A'];
      if (f->is_toggle) {
        const char *next = (strcmp(f->value, "ON") == 0) ? "OFF" : "ON";
        strncpy(f->value, next, (unsigned)f->max_len);
        f->value[f->max_len] = '\0';
        f->current_len = (int)strlen(f->value);
        f->dirty = 1;
      } else {
        ui_edit_field_single(f);
      }
    }
  }
}
