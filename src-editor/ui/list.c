/* CONFIGURE UI - Paginated list display. */
#include <conio.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "ui.h"

ui_list_state_t ui_list_init_state(int count, int per_page)
{
  ui_list_state_t state;
  
  state.total_items = count;
  state.items_per_page = per_page;
  state.current_page = 0;
  state.selected_idx = -1;
  
  if (count == 0) {
    state.total_pages = 1;
  } else {
    state.total_pages = (count + per_page - 1) / per_page;
  }
  
  return state;
}

int ui_list_page_start(const ui_list_state_t *state)
{
  return state->current_page * state->items_per_page;
}

int ui_list_page_end(const ui_list_state_t *state)
{
  int end = (state->current_page + 1) * state->items_per_page;
  if (end > state->total_items) {
    end = state->total_items;
  }
  return end;
}

int ui_list_paged(const char *title, const ui_list_item_t *items, int count, int per_page)
{
  ui_list_state_t state;
  char valid_chars[16];

  if (count == 0) {
    ui_screen_header(title);
    printf("NO ITEMS TO DISPLAY.\n\n");
    printf("PRESS ANY KEY...\n");
    getchar();
    return -1;
  }
  
  state = ui_list_init_state(count, per_page);
  
  while (1) {
    int start, end, i;
    char input;

    ui_screen_header(title);

    start = ui_list_page_start(&state);
    end = ui_list_page_end(&state);
    
    for (i = start; i < end; i++) {
      printf("%2d. %s\n", i + 1, items[i].label);
    }
    
    printf("\n");
    
    strcpy(valid_chars, "Q");
    if (state.current_page > 0) {
      strcat(valid_chars, "P");
      ui_hotkey_label('P', "PREV");
    }
    if (state.current_page < state.total_pages - 1) {
      strcat(valid_chars, "N");
      ui_hotkey_label('N', "NEXT");
    }
    ui_hotkey_label('Q', "QUIT");
    printf("\n\n");

    input = ui_menu_input("CHOICE:", valid_chars);
    
    if (input == 'N' && state.current_page < state.total_pages - 1) {
      state.current_page++;
    } else if (input == 'P' && state.current_page > 0) {
      state.current_page--;
    } else if (input == 'Q') {
      return -1;
    }
  }
  
  return state.selected_idx;
}
