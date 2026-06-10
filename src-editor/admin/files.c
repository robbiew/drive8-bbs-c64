/**
 * CONFIGURE — File Areas Admin Module
 *
 * List, create, edit, delete upload/download areas.
 * Mirrors admin/messages.c patterns.
 */

#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "bbs/file_areas.h"
#include "bbs/err.h"
#include "ui/ui.h"
#include "admin/admin.h"

#define AREAS_PER_PAGE 4

/* -------------------------------------------------------------------------
 * Page buffer
 * ---------------------------------------------------------------------- */
static ud_area_record_t s_apage[AREAS_PER_PAGE];
static u8 s_apage_count;

static void apage_load(u8 page, u8 device)
{
  u8 i;
  s_apage_count = 0;
  for (i = 0; i < AREAS_PER_PAGE; i++) {
    u8 idx = (u8)(page * AREAS_PER_PAGE + i + 1);
    if (file_area_by_index(idx, &s_apage[i], device) == BBS_OK) {
      s_apage_count++;
    } else {
      break;
    }
  }
}

static void apage_show(u8 page, u8 total)
{
  u8 i;
  ui_screen_header("FILE AREAS");
  printf(" #  TITLE                RD UL\n");
  for (i = 0; i < s_apage_count; i++) {
    char t[21];
    u8 j;
    for (j = 0; j < 20; j++) {
      char c = s_apage[i].title[j];
      t[j] = (c == 0) ? ' ' : c;
    }
    t[20] = '\0';
    printf(" %u  %-20s %u  %u\n",
      (unsigned)(i + 1),
      t,
      (unsigned)s_apage[i].access_level,
      (unsigned)s_apage[i].upload_level);
  }
  printf("\n");
  printf("PAGE %u  TOTAL %u\n", (unsigned)(page + 1), (unsigned)total);
}

/* -------------------------------------------------------------------------
 * Pick an area by number (returns BBS_OK and fills *out)
 * ---------------------------------------------------------------------- */
static bbs_err_t areas_pick(const char *prompt, u8 device, ud_area_record_t *out)
{
  u8 total = file_area_count(device);
  u8 total_pages, page = 0;
  char nbuf[4];
  int nlen;

  if (total == 0) {
    ui_error("NO FILE AREAS DEFINED.");
    return BBS_ENOTFOUND;
  }

  total_pages = (u8)((total + AREAS_PER_PAGE - 1) / AREAS_PER_PAGE);

  for (;;) {
    char ch;
    apage_load(page, device);
    apage_show(page, total);
    ui_hotkey_label('N', "NEXT");
    ui_hotkey_label('P', "PREV");
    ui_hotkey_label('Q', "CANCEL");
    printf("\n\n");
    ui_print_centered(prompt);
    printf("# TO SELECT: ");
    ch = (char)toupper((unsigned char)getch());
    printf("\n");

    if (ch == 'Q') return BBS_ENOTFOUND;
    if (ch == 'N') { if (page < total_pages - 1) page++; continue; }
    if (ch == 'P') { if (page > 0) page--; continue; }

    if (ch >= '1' && ch <= '6') {
      u8 sel = (u8)(ch - '0');
      if (sel <= s_apage_count) {
        *out = s_apage[sel - 1];
        return BBS_OK;
      }
    }

    /* Multi-digit number */
    if (ch >= '0' && ch <= '9') {
      nbuf[0] = ch; nlen = 1;
      for (;;) {
        ch = getchar();
        if (ch == 13 || ch == '\n') { printf("\n"); break; }
        if (ch == 20 && nlen > 0) { nlen--; printf("\x08 \x08"); continue; }
        if (ch >= '0' && ch <= '9' && nlen < 3) { nbuf[nlen++] = ch; }
      }
      nbuf[nlen] = '\0';
      if (nlen > 0) {
        u8 idx = (u8)atoi(nbuf);
        if (file_area_by_index(idx, out, device) == BBS_OK) return BBS_OK;
        ui_error("AREA NOT FOUND.");
      }
    }
  }
}

/* -------------------------------------------------------------------------
 * Edit buffers
 * ---------------------------------------------------------------------- */
static char s_edit_title[21];
static char s_edit_read[2];
static char s_edit_upload[2];

static const char *validate_level_0_5(const char *val, void *ctx)
{
  (void)ctx;
  if (val[0] < '0' || val[0] > '5' || val[1] != '\0')
    return "LEVEL MUST BE 0-5.";
  return 0;
}

/* -------------------------------------------------------------------------
 * LIST
 * ---------------------------------------------------------------------- */
static void areas_do_list(u8 device)
{
  u8 total = file_area_count(device);
  u8 total_pages, page = 0;

  if (total == 0) {
    ui_error("NO FILE AREAS DEFINED.");
    return;
  }

  total_pages = (u8)((total + AREAS_PER_PAGE - 1) / AREAS_PER_PAGE);

  for (;;) {
    char ch;
    apage_load(page, device);
    apage_show(page, total);
    ui_hotkey_label('N', "NEXT");
    ui_hotkey_label('P', "PREV");
    ui_hotkey_label('Q', "BACK");
    printf("\n\nCMD?:");
    ch = (char)toupper((unsigned char)getch());
    printf("\n");
    if (ch == 'Q') return;
    if (ch == 'N' && page < total_pages - 1) page++;
    if (ch == 'P' && page > 0) page--;
  }
}

/* -------------------------------------------------------------------------
 * CREATE
 * ---------------------------------------------------------------------- */
static void areas_do_create(u8 device)
{
  char title[21];
  char ch;
  int len;
  u8 new_id = 0;
  bbs_err_t err;

  ui_screen_header("CREATE FILE AREA");

  /* Title */
  printf("TITLE (MAX 20 CHARS): ");
  len = ui_read_line(title, 20, UI_CASE_MIXED);
  printf("\n");
  if (len == 0) { ui_error("TITLE CANNOT BE EMPTY."); return; }
  title[len] = '\0';

  /* Read level */
  printf("MIN DOWNLOAD LEVEL (0-5): ");
  ch = getchar(); printf("\n");
  if (ch < '0' || ch > '5') { ui_error("INVALID LEVEL."); return; }
  s_edit_read[0] = ch; s_edit_read[1] = 0;

  /* Upload level */
  printf("MIN UPLOAD LEVEL (0-5): ");
  ch = getchar(); printf("\n");
  if (ch < '0' || ch > '5') { ui_error("INVALID LEVEL."); return; }
  s_edit_upload[0] = ch; s_edit_upload[1] = 0;

  /* Confirm */
  ui_screen_header("CONFIRM CREATE AREA");
  printf("TITLE:    %s\n", title);
  printf("DOWNLOAD: %c\n", s_edit_read[0]);
  printf("UPLOAD:   %c\n", s_edit_upload[0]);
  printf("\n");
  ui_hotkey_label('Y', "CREATE");
  ui_hotkey_label('N', "CANCEL");
  printf("\n\nCMD?:");
  ch = (char)toupper((unsigned char)getch());
  printf("\n");
  if (ch != 'Y') { printf("CANCELLED.\n"); return; }

  err = file_area_create(title,
    (u8)(s_edit_read[0]   - '0'),
    (u8)(s_edit_upload[0] - '0'),
    device, &new_id);
  if (err != BBS_OK) {
    ui_op_error("CREATE", (u8)err); return;
  }

  printf("\nAREA CREATED (ID=%u).\n", (unsigned)new_id);
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * EDIT
 * ---------------------------------------------------------------------- */
static void areas_do_edit(u8 device)
{
  ud_area_record_t area;
  ui_edit_field_t fields[3];
  bbs_err_t err;
  int result, i;
  char title[32];

  if (areas_pick("SELECT AREA TO EDIT", device, &area) != BBS_OK)
    return;

  strncpy(s_edit_title, area.title, 20); s_edit_title[20] = '\0';
  for (i = 19; i >= 0 && (s_edit_title[i] == ' ' || s_edit_title[i] == 0); i--)
    s_edit_title[i] = '\0';

  s_edit_read[0]   = (char)('0' + (area.access_level > 5 ? 0 : area.access_level));
  s_edit_read[1]   = '\0';
  s_edit_upload[0] = (char)('0' + (area.upload_level > 5 ? 0 : area.upload_level));
  s_edit_upload[1] = '\0';

  memset(fields, 0, sizeof(fields));   /* zero is_toggle + all members up front */
  fields[0].label = "TITLE";           fields[0].value = s_edit_title;  fields[0].max_len = 20; fields[0].current_len = (int)strlen(s_edit_title);
  fields[0].dirty = 0; fields[0].validate = NULL; fields[0].validate_ctx = NULL; fields[0].case_mode = UI_CASE_MIXED;
  fields[1].label = "DLOAD LVL (0-5)"; fields[1].value = s_edit_read;   fields[1].max_len = 1;  fields[1].current_len = 1;
  fields[1].dirty = 0; fields[1].validate = validate_level_0_5; fields[1].validate_ctx = NULL; fields[1].case_mode = UI_CASE_UPPER;
  fields[2].label = "UPLD LVL (0-5)";  fields[2].value = s_edit_upload; fields[2].max_len = 1;  fields[2].current_len = 1;
  fields[2].dirty = 0; fields[2].validate = validate_level_0_5; fields[2].validate_ctx = NULL; fields[2].case_mode = UI_CASE_UPPER;

  sprintf(title, "EDIT AREA: %s", area.title);
  result = ui_edit_form(title, fields, 3);
  if (result == -1) return;

  if (s_edit_title[0] == '\0' || s_edit_title[0] == ' ') {
    ui_error("TITLE CANNOT BE EMPTY."); return;
  }
  if (s_edit_read[0] < '0' || s_edit_read[0] > '5') {
    ui_error("INVALID DOWNLOAD LEVEL (0-5)."); return;
  }
  if (s_edit_upload[0] < '0' || s_edit_upload[0] > '5') {
    ui_error("INVALID UPLOAD LEVEL (0-5)."); return;
  }

  strncpy(area.title, s_edit_title, 20);
  area.access_level = (u8)(s_edit_read[0]   - '0');
  area.upload_level = (u8)(s_edit_upload[0] - '0');

  err = file_area_save(&area, device);
  if (err != BBS_OK) {
    ui_op_error("SAVE", (u8)err); return;
  }

  printf("\nAREA SAVED.\n");
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * DELETE
 * ---------------------------------------------------------------------- */
static void areas_do_delete(u8 device)
{
  ud_area_record_t area;
  char prompt[40];
  bbs_err_t err;

  if (areas_pick("SELECT AREA TO DELETE", device, &area) != BBS_OK)
    return;

  sprintf(prompt, "DELETE AREA %u: %s?", (unsigned)area.id, area.title);
  if (!ui_confirm(prompt)) return;

  err = file_area_delete(area.id, device);
  if (err != BBS_OK) {
    ui_op_error("DELETE", (u8)err); return;
  }

  printf("\nAREA DELETED.\n");
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * Public entry point
 * ---------------------------------------------------------------------- */
void admin_files_menu(u8 device)
{
  static const ui_menu_item_t items[] = {
    { 'L', "LIST AREAS"  },
    { 'C', "CREATE AREA" },
    { 'E', "EDIT AREA"   },
    { 'D', "DELETE AREA" },
    { 'Q', "BACK"        },
  };

  for (;;) {
    char ch;
    ui_menu_display("FILE AREAS", items, 5);
    ch = ui_menu_input("CHOICE:", "LCEDQ");
    switch (ch) {
      case 'L': areas_do_list(device);   break;
      case 'C': areas_do_create(device); break;
      case 'E': areas_do_edit(device);   break;
      case 'D': areas_do_delete(device); break;
      case 'Q': return;
    }
  }
}
