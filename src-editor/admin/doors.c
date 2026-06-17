/**
 * CONFIGURE — Door Programs Admin Module
 *
 * List, create, edit, delete door program slots (1..DOORS_MAX).
 */

#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "bbs/records.h"
#include "bbs/err.h"
#include "ui/ui.h"
#include "admin/admin.h"

/* Forward-declare only the data-layer functions needed here; avoids pulling in
 * bbs/doors.h → bbs/session.h which is BOOT-only and not linked in CONFIGURE. */
u8        door_count(u8 device);
bbs_err_t door_by_id(u8 id, door_record_t *out, u8 device);
bbs_err_t door_save(const door_record_t *rec, u8 device);
bbs_err_t door_delete(u8 id, u8 device);

/* -------------------------------------------------------------------------
 * Edit a door record field-by-field (sequential input, no form widget).
 * Used for both CREATE and EDIT.
 * ---------------------------------------------------------------------- */
static void doors_edit(door_record_t *d, u8 device)
{
  char buf[17];
  char ch;
  int len;
  bbs_err_t err;

  ui_screen_header("EDIT DOOR");
  printf("SLOT: %u\n\n", (unsigned)d->id);

  /* Title (mixed case) */
  printf("TITLE (16): ");
  len = ui_read_line(buf, 16, UI_CASE_MIXED);
  printf("\n");
  if (len > 0) { strncpy(d->title, buf, 16); }

  /* Filename (uppercase) */
  printf("FILENAME (16): ");
  len = ui_read_line(buf, 16, UI_CASE_UPPER);
  printf("\n");
  if (len > 0) { strncpy(d->filename, buf, 16); }

  /* Device */
  printf("DEVICE (8-30, ENTER=KEEP %u): ", (unsigned)d->device);
  len = ui_read_line(buf, 2, UI_CASE_UPPER);
  printf("\n");
  if (len > 0) {
    u8 v = (u8)atoi(buf);
    if (v >= 8 && v <= 30) d->device = v;
  }

  /* Drive */
  printf("DRIVE (0-1, ENTER=KEEP %u): ", (unsigned)d->drive);
  ch = getch(); printf("\n");
  if (ch == '0' || ch == '1') d->drive = (u8)(ch - '0');

  /* cmd_key */
  printf("CMD KEY (A-Z or - NONE, ENTER=KEEP %c): ",
    d->cmd_key ? d->cmd_key : '-');
  ch = (char)toupper((unsigned char)getch()); printf("\n");
  if (ch == '-') d->cmd_key = 0;
  else if (ch >= 'A' && ch <= 'Z') d->cmd_key = ch;

  /* min_level */
  printf("MIN LEVEL (0-5, ENTER=KEEP %u): ", (unsigned)d->min_level);
  ch = getch(); printf("\n");
  if (ch >= '0' && ch <= '5') d->min_level = (u8)(ch - '0');

  /* login_order */
  printf("LOGIN ORDER (0=NONE, ENTER=KEEP %u): ", (unsigned)d->login_order);
  len = ui_read_line(buf, 3, UI_CASE_UPPER);
  printf("\n");
  if (len > 0) d->login_order = (u8)atoi(buf);

  /* Flags toggles */
  printf("ENABLED? (Y/N, ENTER=KEEP %s): ",
    (d->flags & DOOR_F_ENABLED) ? "Y" : "N");
  ch = (char)toupper((unsigned char)getch()); printf("\n");
  if (ch == 'Y') d->flags |= DOOR_F_ENABLED;
  else if (ch == 'N') d->flags &= (u8)(~DOOR_F_ENABLED);

  printf("RUN AT LOGIN? (Y/N, ENTER=KEEP %s): ",
    (d->flags & DOOR_F_LOGIN) ? "Y" : "N");
  ch = (char)toupper((unsigned char)getch()); printf("\n");
  if (ch == 'Y') d->flags |= DOOR_F_LOGIN;
  else if (ch == 'N') d->flags &= (u8)(~DOOR_F_LOGIN);

  /* Save */
  printf("\n");
  if (!ui_confirm("SAVE DOOR?")) return;

  err = door_save(d, device);
  if (err != BBS_OK) { ui_op_error("SAVE", (u8)err); return; }
  printf("DOOR SAVED.\n");
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * LIST — show all defined doors
 * ---------------------------------------------------------------------- */
static void doors_do_list(u8 device)
{
  u8 i, found = 0;
  ui_screen_header("DOOR PROGRAMS");
  for (i = 1; i <= DOORS_MAX; i++) {
    door_record_t r;
    if (door_by_id(i, &r, device) == BBS_OK) {
      printf("%2u %c %s %-12.12s\n",
        (unsigned)i,
        r.cmd_key ? r.cmd_key : '-',
        (r.flags & DOOR_F_ENABLED) ? "Y" : "N",
        r.title);
      found = 1;
    }
  }
  if (!found) printf("NO DOORS DEFINED.\n");
  printf("\n");
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * Pick a door slot: show all defined doors then ask for slot number.
 * Returns 0 if cancelled, else the door id.
 * ---------------------------------------------------------------------- */
static u8 doors_pick(u8 device)
{
  char buf[3];
  int len;
  u8 i, idx;
  door_record_t r;

  ui_screen_header("DOOR PROGRAMS");
  for (i = 1; i <= DOORS_MAX; i++) {
    if (door_by_id(i, &r, device) == BBS_OK)
      printf("%2u %-14.14s\n", (unsigned)i, r.title);
  }
  printf("\n");
  ui_hotkey_label('Q', "CANCEL");
  printf("\nSLOT # (Q=CANCEL): ");
  len = ui_read_line(buf, 2, UI_CASE_UPPER);
  printf("\n");
  if (!len || buf[0] == 'Q') return 0;
  idx = (u8)atoi(buf);
  if (door_by_id(idx, &r, device) == BBS_OK) return idx;
  ui_error("NOT FOUND.");
  return 0;
}

/* -------------------------------------------------------------------------
 * CREATE
 * ---------------------------------------------------------------------- */
static void doors_do_create(u8 device)
{
  door_record_t rec;
  u8 slot = 0, i;

  for (i = 1; i <= DOORS_MAX; i++) {
    if (door_by_id(i, &rec, device) != BBS_OK) { slot = i; break; }
  }
  if (!slot) { ui_error("DOOR SLOTS FULL."); return; }

  memset(&rec, 0, sizeof(rec));
  rec.id     = slot;
  rec.flags  = DOOR_F_ENABLED;
  rec.device = 10;
  doors_edit(&rec, device);
}

/* -------------------------------------------------------------------------
 * EDIT
 * ---------------------------------------------------------------------- */
static void doors_do_edit(u8 device)
{
  door_record_t door;
  u8 id = doors_pick(device);
  if (!id) return;
  if (door_by_id(id, &door, device) != BBS_OK) { ui_error("NOT FOUND."); return; }
  doors_edit(&door, device);
}

/* -------------------------------------------------------------------------
 * DELETE
 * ---------------------------------------------------------------------- */
static void doors_do_delete(u8 device)
{
  door_record_t door;
  u8 id = doors_pick(device);
  bbs_err_t err;
  char prompt[24];

  if (!id) return;
  if (door_by_id(id, &door, device) != BBS_OK) { ui_error("NOT FOUND."); return; }

  sprintf(prompt, "DELETE DOOR %u?", (unsigned)id);
  if (!ui_confirm(prompt)) return;

  err = door_delete(id, device);
  if (err != BBS_OK) { ui_op_error("DELETE", (u8)err); return; }
  printf("DOOR DELETED.\n");
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * Public entry point
 * ---------------------------------------------------------------------- */
void admin_doors_menu(u8 device)
{
  static const ui_menu_item_t items[] = {
    { 'L', "LIST DOORS"  },
    { 'C', "CREATE DOOR" },
    { 'E', "EDIT DOOR"   },
    { 'D', "DELETE DOOR" },
    { 'Q', "BACK"        },
  };

  for (;;) {
    char ch;
    ui_menu_display("DOOR PROGRAMS", items, 5);
    ch = ui_menu_input("CHOICE:", "LCEDQ");
    switch (ch) {
      case 'L': doors_do_list(device);   break;
      case 'C': doors_do_create(device); break;
      case 'E': doors_do_edit(device);   break;
      case 'D': doors_do_delete(device); break;
      case 'Q': return;
    }
  }
}
