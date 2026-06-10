/* CONFIGURE UI - Confirmation dialogs and status displays. */
#include <conio.h>
#include <stdio.h>
#include <ctype.h>
#include "ui.h"

void ui_press_any_key(void)
{
  printf("PRESS ANY KEY...\n");
  getch();
}

char ui_page_prompt(u8 page, u8 total_pages)
{
  char ch;
  (void)page; (void)total_pages;
  ui_hotkey_label('N', "NEXT");
  ui_hotkey_label('P', "PREV");
  ui_hotkey_label('Q', "BACK");
  printf("\n\nCMD?:");
  ch = (char)toupper((unsigned char)getch());
  printf("\n");
  return ch;
}

u8 ui_confirm(const char *prompt)
{
  ui_screen_header(prompt);
  printf("THIS CANNOT BE UNDONE.\n\n");
  ui_hotkey_label('Y', "YES");
  ui_hotkey_label('N', "NO");
  printf("\n\n");

  while (1) {
    char input;
    printf("CMD?:");
    input = (char)toupper((unsigned char)getch());
    printf("\n");
    if (input == 'Y') return 1;
    if (input == 'N') return 0;
    ui_beep();
  }
}

void ui_status(const char *title, const char **lines, int count)
{
  int i;

  ui_screen_header(title);

  for (i = 0; i < count; i++) {
    printf("%s\n", lines[i]);
  }

  printf("\n");
  printf("PRESS ANY KEY TO CONTINUE...\n");
  getch();
}

void ui_error(const char *message)
{
  const char *lines[1];
  lines[0] = message;
  ui_status("** ERROR **", lines, 1);
}

void ui_op_error(const char *op, u8 code)
{
  char buf[32];
  sprintf(buf, "%s FAILED (CODE %u)", op, (unsigned)code);
  ui_error(buf);
}
