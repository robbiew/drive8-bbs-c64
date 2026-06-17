/* CONFIGURE UI Framework - Common types and utilities for SysOp interface. */
#ifndef CONFIGURE_UI_UI_H
#define CONFIGURE_UI_UI_H

#include "bbs/types.h"

/* Common UI dimensions and constants */
#define UI_MENU_MAX_ITEMS    16
#define UI_LIST_PER_PAGE     10
#define UI_FIELD_MAX_LEN     40
#define UI_CONFIRM_RETRIES   3
#define UI_SCREEN_WIDTH      40

/* App header shown in the green bar at the top of every editor screen */
#define UI_APP_HDR "TURBO/64 BBS CONFIGURATION"

/* Per-field input case handling */
#define UI_CASE_UPPER  0   /* normalize input to uppercase (default) */
#define UI_CASE_MIXED  1   /* preserve case as typed */

/* Menu item structure */
typedef struct {
  char key;                    /* Single-char menu key */
  const char *label;           /* Menu option label */
} ui_menu_item_t;

/* List item structure (for paginated lists) */
typedef struct {
  char label[40];              /* Displayed text */
  void *data;                  /* Associated data pointer */
} ui_list_item_t;

/* Edit field structure (for forms) */
typedef struct {
  const char *label;           /* Field name (max 16 chars) */
  char *value;                 /* Pointer to value buffer */
  int max_len;                 /* Max input length */
  int current_len;             /* Current value length */
  int dirty;                   /* Non-zero if value edited but not yet saved */
  /* Optional validator: returns NULL if OK, or a short error message string. */
  const char *(*validate)(const char *new_value, void *ctx);
  void *validate_ctx;
  u8 case_mode;                /* UI_CASE_UPPER (default) | UI_CASE_MIXED */
  u8 is_toggle;                /* Non-zero: selecting the field flips its value
                                  between "OFF" and "ON" instead of opening the
                                  line editor (boolean toggle field). */
} ui_edit_field_t;

/* List pagination state */
typedef struct {
  int total_items;
  int items_per_page;
  int current_page;
  int total_pages;
  int selected_idx;
} ui_list_state_t;

/* Core UI functions - util.c */
void ui_clear_screen(void);
void ui_print_centered(const char *text);
void ui_print_header_bar(const char *text);
void ui_print_hotkey(char key);
/* Inline reverse-hotkey + green label for footers/action prompts, e.g. "N NEXT". */
void ui_hotkey_label(char key, const char *label);
/* One edit-screen field label: " <key> LABEL   : " (reverse hotkey, green
 * 8-wide label, colon), leaving color white for the caller to print the value. */
void ui_edit_label(char key, const char *label);
/* Clear screen, draw the green app-header bar, then the centered screen title. */
void ui_screen_header(const char *title);
void ui_print_line(const char *text);
void ui_print_separator(void);
void ui_beep(void);

/* Menu functions - menu.c */
void ui_menu_display(const char *title, const ui_menu_item_t *items, int count);
char ui_menu_input(const char *prompt, const char *valid_chars);

/* List functions - list.c */
int ui_list_paged(const char *title, const ui_list_item_t *items, int count, int per_page);
ui_list_state_t ui_list_init_state(int count, int per_page);
int ui_list_page_start(const ui_list_state_t *state);
int ui_list_page_end(const ui_list_state_t *state);

/* Edit functions - edit.c */
/* Read one line of input into buf (capacity must be >= max+1). Raw getch()
 * input with manual echo: UPPER fields (case_mode == UI_CASE_UPPER) are
 * touppered before echo, so they display uppercase while typing. Handles
 * DEL/BS, echoes the terminating newline. Returns the final length. */
int ui_read_line(char *buf, int max, u8 case_mode);
/* Backspace-aware unsigned-number prompt (1-3 digits). Returns -1 if left blank
 * (caller keeps current), else 0..255. Echoes the prompt and input. */
int ui_read_num(const char *prompt);
int ui_edit_form(const char *title, ui_edit_field_t *fields, int count);
void ui_edit_field_single(ui_edit_field_t *field);
/* Select a value from a fixed list. Updates value buffer with the chosen option. */
void ui_select_field(const char *label, char *value, int max_len,
                     const char **options, int num_options);

/* Dialog functions - dialog.c */
u8 ui_confirm(const char *prompt);
void ui_status(const char *title, const char **lines, int count);
void ui_error(const char *message);
void ui_press_any_key(void);
char ui_page_prompt(u8 page, u8 total_pages);
void ui_op_error(const char *op, u8 code);

#endif /* CONFIGURE_UI_UI_H */
