/* CONFIGURE UI - Common screen utilities. */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

void ui_clear_screen(void)
{
  clrscr();
}

void ui_print_line(const char *text)
{
  printf("%s\n", text);
}

void ui_print_separator(void)
{
  printf("========================================\n");
}

void ui_print_centered(const char *text)
{
  int len = strlen(text);
  int padding = (UI_SCREEN_WIDTH - len) / 2;
  int i;
  
  for (i = 0; i < padding; i++) {
    printf(" ");
  }
  printf("%s\n", text);
}

void ui_print_header_bar(const char *text)
{
  int len = (int)strlen(text);
  int padding = (UI_SCREEN_WIDTH - len) / 2;
  int i;

  if (padding < 0) padding = 0;

  textcolor(COLOR_LT_GREEN);
  revers(1);
  for (i = 0; i < padding; i++) putchar(' ');
  printf("%s", text);
  /* Fill the rest of the 40-col row so the whole line is a solid green bar.
   * Printing exactly UI_SCREEN_WIDTH chars wraps the cursor to the next line,
   * so no explicit newline is emitted here. */
  for (i = padding + len; i < UI_SCREEN_WIDTH; i++) putchar(' ');
  revers(0);
  textcolor(COLOR_WHITE);
}

void ui_screen_header(const char *title)
{
  ui_clear_screen();
  ui_print_header_bar(UI_APP_HDR);
  printf("\n");
  textcolor(COLOR_WHITE);
  ui_print_centered(title);
  printf("\n");
}

/* Yellow so the hotkey stands out against the green label. */
void ui_print_hotkey(char key)
{
  textcolor(COLOR_YELLOW);
  revers(1);
  putchar(key);
  revers(0);
}

void ui_hotkey_label(char key, const char *label)
{
  ui_print_hotkey(key);
  textcolor(COLOR_LT_GREEN);
  printf(" %s  ", label);
  textcolor(COLOR_WHITE);
}

void ui_beep(void)
{
  printf("\x07");
}
