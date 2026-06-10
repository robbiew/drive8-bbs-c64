/* CONFIGURE UI - Menu display and input. */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ui.h"

void ui_menu_display(const char *title, const ui_menu_item_t *items, int count)
{
  int rows = (count + 1) / 2;   /* left column gets the extra item on odd counts */
  int r;

  ui_screen_header(title);

  /* Column-major 2-column grid: rev-hotkey + green label.
   * Left column = items[0..rows-1], right column = items[rows..count-1]. */
  for (r = 0; r < rows; r++) {
    int right = r + rows;

    putchar(' ');
    ui_print_hotkey(items[r].key);
    textcolor(COLOR_LT_GREEN);
    printf(" %-13s", items[r].label);

    if (right < count) {
      ui_print_hotkey(items[right].key);
      textcolor(COLOR_LT_GREEN);
      printf(" %s", items[right].label);
    }

    textcolor(COLOR_WHITE);
    printf("\n");
  }

  printf("\n");
}

char ui_menu_input(const char *prompt, const char *valid_chars)
{
  (void)prompt;  /* hardcoded CMD?: prompt; caller-supplied string is unused */
  textcolor(COLOR_WHITE);
  printf("CMD?:");

  while (1) {
    /* Single keypress: getch() = GETIN (no RETURN needed) and does NOT echo,
     * so we control the echo. It still applies iocharmap, so an unshifted key
     * returns ASCII; upcasing then matches the uppercase menu labels. */
    char input = (char)toupper((unsigned char)getch());

    /* Ignore a stray RETURN (muscle memory from the old KEY+RETURN menus). */
    if (input == '\r' || input == '\n') continue;

    if (input == 27) {
      printf("\n");
      return 27;
    }

    if (strchr(valid_chars, input) != NULL) {
      putchar(input);   /* echo the matched key in UPPERCASE */
      printf("\n");
      return input;
    }

    ui_beep();          /* invalid key: just beep, nothing echoed */
  }
}

