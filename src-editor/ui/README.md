# CONFIGURE UI Framework

Reusable UI component library for the TURBO/64 BBS SysOp editor. Provides a consistent, full-screen text-based interface for the CONFIGURE admin tool running on Commodore 64.

## Overview

The UI framework is designed for a 40-column text-only display with no dynamic allocation. All UI components:
- Use static buffers (no malloc/free)
- Emit uppercase/graphics mode control (\x8e) at start
- Support single-character menu navigation
- Work with conio.h and stdio.h

## File Organization

```
src-editor/ui/
├── ui.h           Common types and function declarations (public API)
├── util.c         Screen utilities (clear, print, separator, centered text)
├── menu.c         Menu display and input handling
├── list.c         Paginated list display with navigation
├── edit.c         Inline field editor for forms
└── dialog.c       Confirmation dialogs and status displays
```

## Components

### util.c — Screen Utilities
- `ui_clear_screen()` — Clear screen using clrscr()
- `ui_print_line(text)` — Print text with newline
- `ui_print_centered(text)` — Center text on 40-column screen
- `ui_print_header_bar(text)` — Full-width green reverse-video app-header bar
- `ui_screen_header(title)` — Clear + green app bar + centered screen title (use at the top of every screen)
- `ui_print_hotkey(key)` — Reverse-video yellow hotkey letter
- `ui_print_separator()` — Print 40-character separator line
- `ui_beep()` — Emit system beep

### menu.c — Menu Display and Input
- `ui_menu_display(title, items, count)` — Display menu with title and items
  - Centered title
  - Separator line
  - Items in format `[KEY] Label`
  - Returns nothing; caller handles input
  
- `ui_menu_input(prompt, valid_chars)` — Get single-key menu input
  - Displays prompt
  - Reads uppercase character
  - Returns character if in valid_chars
  - Loops on invalid input
  - Returns ESC (27) to cancel

**Example:**
```c
ui_menu_item_t menu[] = {
  {'L', "LIST USERS"},
  {'E', "EDIT USER"},
  {'Q', "QUIT"}
};
ui_menu_display("USER MANAGEMENT", menu, 3);
char choice = ui_menu_input("CHOICE:", "LEQ");
```

### list.c — Paginated Lists
- `ui_list_init_state(count, per_page)` → `ui_list_state_t` — Initialize pagination
- `ui_list_page_start(state)` → starting index for current page
- `ui_list_page_end(state)` → ending index for current page
- `ui_list_paged(title, items, count, per_page)` → selected index or -1 (quit)
  - Displays title and separator
  - Shows current page of items (N per page)
  - Navigation: [N] NEXT, [P] PREV, [Q] QUIT
  - Auto-hides unavailable buttons (no NEXT on last page, etc.)

**Example:**
```c
ui_list_item_t users[10] = {
  {"SYSOP"},
  {"ALICE"},
  ...
};
int selected = ui_list_paged("LIST USERS", users, 10, 5);
if (selected >= 0) {
  // User selected item at index `selected`
}
```

### edit.c — Inline Field Editor
- `ui_edit_field_single(field)` — Edit a single field in-place
  - Displays field name and current value in brackets
  - Supports backspace (DEL key, 0x14)
  - ESC cancels without saving
  - ENTER confirms
  
- `ui_edit_form(title, fields, count)` → 0 (save), -1 (cancel)
  - Displays title
  - Shows all fields with a reverse-video letter hotkey (A, B, C…)
  - Menu: letter edits that field, [S] save, [Q] back
  - Returns 0 on save, -1 on cancel

**Example:**
```c
char handle_buf[16];
char access_buf[2];

ui_edit_field_t fields[] = {
  {.label = "HANDLE", .value = handle_buf, .max_len = 15, .current_len = 5},
  {.label = "ACCESS", .value = access_buf, .max_len = 1, .current_len = 1}
};

if (ui_edit_form("EDIT USER", fields, 2) == 0) {
  // Save changes: fields[0].value and fields[1].value updated
}
```

### dialog.c — Confirmation Dialogs
- `ui_confirm(prompt)` → 1 (yes), 0 (no)
  - Displays prompt
  - Asks [Y] YES, DELETE / [N] NO, CANCEL
  - Returns user choice
  
- `ui_status(title, lines, count)` — Multi-line status display
  - Displays title and separator
  - Prints all lines
  - "PRESS ANY KEY TO CONTINUE"
  - Blocks until key pressed
  
- `ui_error(message)` — Quick error display
  - Wrapper around ui_status with "ERROR" title

**Example:**
```c
if (ui_confirm("DELETE USER: ALICE (ID=2)?")) {
  // User confirmed deletion
  user_delete(2);
}
```

## Types

### ui_menu_item_t
```c
typedef struct {
  char key;           // Single-char menu key
  const char *label;  // Menu option label
} ui_menu_item_t;
```

### ui_list_item_t
```c
typedef struct {
  char label[40];     // Displayed text
  void *data;         // Associated data pointer (e.g., user_record_t*)
} ui_list_item_t;
```

### ui_edit_field_t
```c
typedef struct {
  const char *label;  // Field name (max 16 chars)
  char *value;        // Pointer to value buffer (caller-provided)
  int max_len;        // Max input length
  int current_len;    // Current value length
} ui_edit_field_t;
```

### ui_list_state_t
```c
typedef struct {
  int total_items;      // Total items in list
  int items_per_page;   // Items displayed per page
  int current_page;     // 0-indexed page number
  int total_pages;      // Calculated total pages
  int selected_idx;     // Selected item index (future use)
} ui_list_state_t;
```

## Constants

- `UI_MENU_MAX_ITEMS` — Max menu items (16)
- `UI_LIST_PER_PAGE` — Default list items per page (10)
- `UI_FIELD_MAX_LEN` — Max field value length (40)
- `UI_SCREEN_WIDTH` — C64 screen width (40)

## Design Notes

### No Dynamic Allocation
All UI functions use static buffers or caller-provided storage. This matches Oscar64/C64 constraints (limited RAM, no malloc on embedded systems).

### Uppercase/Graphics Mode
All output includes `\x8e` to ensure the C64 displays uppercase characters correctly. Lowercase bytes 0x61–0x7A render as graphics in uppercase/graphics mode.

### Single-Character Input
All menu/dialog input is single-key, uppercase-only. No string input to keep the interface simple and responsive on 1200-baud connections.

### ABORT / ESC Handling
Where appropriate, ESC (27) is used to cancel or go back. This matches historical C64 BBS conventions.

### Pagination
Lists automatically calculate total pages and hide navigation buttons that are not applicable (e.g., no [NEXT] on last page).

## Integration with CONFIGURE

The UI framework is used by `src-editor/setup.c` to implement the admin menu:

```c
ui_menu_item_t menu_items[4] = {
  {'I', "INITIALIZE DISK"},
  {'M', "MANAGE USERS"},
  {'C', "CONFIGURE SETTINGS"},
  {'Q', "QUIT"}
};

ui_menu_display("T64 SETUP", menu_items, 4);
char choice = ui_menu_input("CHOICE:", "IMCQ");
```

Future modules (user_mgmt.c, settings.c, etc.) can reuse these components to build feature-specific interfaces.

## Compiling

The UI framework is compiled into the CONFIGURE binary via the Makefile:

```makefile
EDITOR_SRCS := src-editor/setup.c src-editor/ui/util.c src-editor/ui/menu.c \
               src-editor/ui/list.c src-editor/ui/edit.c src-editor/ui/dialog.c
```

Build with:
```bash
make editor    # Build CONFIGURE with UI framework
make all       # Build both BOOT and CONFIGURE
```

## Testing in VICE

Launch CONFIGURE in the VICE C64 emulator:

```bash
x64sc -autostart build/c64/CONFIGURE-0.1.0.prg
```

### Test Checklist

- [ ] Menu displays centered title with separator
- [ ] Menu items render in format `[KEY] Label`
- [ ] Menu input accepts uppercase keys and rejects invalid
- [ ] ESC cancels menu (returns 27)
- [ ] List displays current page items with numbering
- [ ] List pagination controls show/hide correctly
- [ ] List [N] and [P] navigate between pages
- [ ] List [Q] returns -1 (quit)
- [ ] Edit form displays all fields with letter hotkeys (A, B, C…)
- [ ] Edit field accepts text input and backspace
- [ ] Edit field ENTER confirms, ESC cancels
- [ ] Edit form [S] and [C] work correctly
- [ ] Dialogs display multi-line text correctly
- [ ] Dialogs accept [Y] and [N] input
- [ ] Screen clears between operations without artifacts

## Future Enhancements

- Input field masks (numeric-only, etc.)
- Scrollable text areas (for long messages)
- Highlight selection in menus/lists
- Help text overlays
- Message box with timeout
- File browser dialog
