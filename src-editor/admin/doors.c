/**
 * CONFIGURE — Door Programs Admin Module
 *
 * List, create, edit, delete door program slots (1..DOORS_MAX).
 * Mirrors the message-areas editor: a paged columnar LIST, a "# TO SELECT"
 * pick screen, and an interactive field-per-line EDIT screen (press a hotkey
 * to edit one field, re-render) with the same color/prompt conventions.
 */

#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bbs/records.h"
#include "bbs/err.h"
#include "ui/ui.h"
#include "admin/admin.h"

/* Forward-declare only the data-layer functions needed here; avoids pulling in
 * bbs/doors.h → bbs/session.h which is BOOT-only and not linked in CONFIGURE. */
u8        door_count(u8 device);
bbs_err_t door_by_index(u8 n, door_record_t *out, u8 device);
bbs_err_t door_by_id(u8 id, door_record_t *out, u8 device);
bbs_err_t door_save(const door_record_t *rec, u8 device);
bbs_err_t door_delete(u8 id, u8 device);

#define DOORS_PER_PAGE  10

/* ------------------------------------------------------------------ */
/* Shared helpers                                                       */
/* ------------------------------------------------------------------ */

/* Copy a 16-byte field into a trimmed, NUL-terminated display string. */
static void door_trim(const char *src, char *dst)
{
  u8 j;
  for (j = 0; j < 16; j++) dst[j] = (src[j] == 0) ? ' ' : src[j];
  dst[16] = '\0';
  for (j = 15; j > 0 && (dst[j] == ' ' || dst[j] == '\0'); j--) dst[j] = '\0';
}

/* Number input and the edit-screen field label are shared helpers in ui/util.c
 * (ui_read_num / ui_edit_label) — same code the message-area editor uses. */

/* ------------------------------------------------------------------ */
/* LIST / pick                                                          */
/* ------------------------------------------------------------------ */

/* Paged door browser.  out==NULL: plain list (Q=DONE).  out!=NULL: pick mode —
 * `prompt` is shown and a slot # (1..page count) selects a door into *out.
 * Renders rows directly via door_by_index (no page cache) to keep CONFIGURE's
 * full main region within budget — doors are few and this is sysop-side. */
static bbs_err_t doors_browse(const char *prompt, u8 device, door_record_t *out)
{
  u8 total = door_count(device);
  u8 total_pages;
  u8 page = 0;

  if (total == 0) { ui_error("NO DOORS DEFINED."); return BBS_ENOTFOUND; }
  total_pages = (u8)((total + DOORS_PER_PAGE - 1) / DOORS_PER_PAGE);

  for (;;) {
    char ch;
    u8 i, shown = 0;
    u8 start = (u8)(page * DOORS_PER_PAGE + 1);
    door_record_t r;

    ui_screen_header("DOOR PROGRAMS");
    printf("PAGE %u/%u\n\n", (unsigned)(page + 1), (unsigned)total_pages);
    printf(" #  TITLE            K DEV LV FL\n");
    for (i = 0; i < DOORS_PER_PAGE; i++) {
      char t[17], fl[3];
      u8 f;
      if (door_by_index((u8)(start + i), &r, device) != BBS_OK) break;
      shown++;
      f = r.flags;
      door_trim(r.title, t);
      fl[0] = (f & DOOR_F_ENABLED) ? 'E' : '-';
      fl[1] = (f & DOOR_F_LOGIN)   ? 'L' : '-';
      fl[2] = '\0';
      printf(" %u  %-16s %c %3u %2u %s\n",
        (unsigned)(i + 1), t, r.cmd_key ? r.cmd_key : '-',
        (unsigned)r.device, (unsigned)r.min_level, fl);
    }
    printf("\n");

    if (page > 0)                     ui_hotkey_label('P', "PREV");
    if (page < (u8)(total_pages - 1)) ui_hotkey_label('N', "NEXT");
    ui_hotkey_label('Q', out ? "BACK" : "DONE");
    if (out) printf("\n\n%s\n# TO SELECT: ", prompt);
    else     printf("\n\nCMD?:");

    ch = (char)toupper((unsigned char)getch());
    printf("\n");

    if (ch == 'Q') return BBS_ENOTFOUND;
    if (ch == 'N' && page < (u8)(total_pages - 1)) { page++; continue; }
    if (ch == 'P' && page > 0)                      { page--; continue; }
    if (out && ch >= '1' && ch <= (char)('0' + shown)) {
      return door_by_index((u8)(start + (ch - '1')), out, device);
    }
    ui_beep();
  }
}

/* ------------------------------------------------------------------ */
/* EDIT screen + loop                                                   */
/* ------------------------------------------------------------------ */

static void doors_show_edit_screen(const door_record_t *d)
{
  char buf[40];
  char t[17];

  sprintf(buf, "EDIT DOOR #%u", (unsigned)d->id);
  ui_screen_header(buf);

  door_trim(d->title, t);
  ui_edit_label('T', "TITLE");    printf("%s\n", t);
  door_trim(d->filename, t);
  ui_edit_label('F', "FILE");     printf("%s\n", t);
  ui_edit_label('D', "DEVICE");   printf("%u\n", (unsigned)d->device);
  ui_edit_label('R', "DRIVE");    printf("%u\n", (unsigned)d->drive);
  ui_edit_label('K', "CMD KEY");  printf("%c\n", d->cmd_key ? d->cmd_key : '-');
  ui_edit_label('L', "MIN LVL");  printf("%u\n", (unsigned)d->min_level);
  ui_edit_label('O', "LOGIN #");
  if (d->login_order == 0) printf("(NONE)\n");
  else                     printf("%u\n", (unsigned)d->login_order);
  ui_edit_label('E', "ENABLED");  printf("%c\n", (d->flags & DOOR_F_ENABLED) ? 'Y' : 'N');
  ui_edit_label('G', "AT LOGIN"); printf("%c\n", (d->flags & DOOR_F_LOGIN)   ? 'Y' : 'N');

  printf("\n ");
  ui_print_hotkey('S'); textcolor(COLOR_LT_GREEN); printf(" SAVE   ");
  ui_print_hotkey('C'); textcolor(COLOR_LT_GREEN); printf(" CANCEL\n");
  textcolor(COLOR_WHITE);
  printf("\n");
}

/* Returns TRUE if the record was saved, FALSE if cancelled. */
static bool_t doors_edit_record(door_record_t *d, u8 device)
{
  for (;;) {
    char ch;
    doors_show_edit_screen(d);
    textcolor(COLOR_WHITE);
    printf("CMD?:");
    ch = (char)toupper((unsigned char)getch());
    printf("\n");

    switch (ch) {
      case 'T': {
        char newt[17];
        int len, i;
        printf("TITLE (16 CHARS MAX): ");
        len = ui_read_line(newt, 16, UI_CASE_MIXED);
        printf("\n");
        if (len > 0) {
          strncpy(d->title, newt, 16);
          for (i = len; i < 16; i++) d->title[i] = 0;
        }
        break;
      }

      case 'F': {
        char newf[17];
        int len, i;
        printf("FILENAME (16 CHARS MAX): ");
        len = ui_read_line(newf, 16, UI_CASE_UPPER);
        printf("\n");
        if (len > 0) {
          strncpy(d->filename, newf, 16);
          for (i = len; i < 16; i++) d->filename[i] = 0;
        }
        break;
      }

      case 'D': {
        int v = ui_read_num("DEVICE (8-30): ");
        if (v < 0) break;                 /* blank = keep */
        if (v >= 8 && v <= 30) d->device = (u8)v;
        else ui_error("INVALID DEVICE.");
        break;
      }

      case 'R': {
        char lc;
        printf("DRIVE (0-1): ");
        lc = getch(); putchar(lc); printf("\n");
        if (lc == '0' || lc == '1') d->drive = (u8)(lc - '0');
        else ui_error("INVALID DRIVE.");
        break;
      }

      case 'K': {
        char lc;
        printf("CMD KEY (A-Z, OR - FOR NONE): ");
        lc = (char)toupper((unsigned char)getch()); putchar(lc); printf("\n");
        if (lc == '-') d->cmd_key = 0;
        else if (lc >= 'A' && lc <= 'Z') d->cmd_key = lc;
        else ui_error("INVALID KEY.");
        break;
      }

      case 'L': {
        char lc;
        printf("MIN LEVEL (0-5): ");
        lc = getch(); putchar(lc); printf("\n");
        if (lc >= '0' && lc <= '5') d->min_level = (u8)(lc - '0');
        else ui_error("INVALID LEVEL.");
        break;
      }

      case 'O': {
        int v = ui_read_num("LOGIN ORDER (0=NONE, 1-255): ");
        if (v < 0) break;                 /* blank = keep */
        d->login_order = (u8)v;
        break;
      }

      case 'E': d->flags ^= DOOR_F_ENABLED; break;
      case 'G': d->flags ^= DOOR_F_LOGIN;   break;

      case 'S': {
        bbs_err_t err;
        if (d->title[0] == 0 || d->title[0] == ' ') { ui_error("TITLE REQUIRED."); break; }
        if (d->filename[0] == 0 || d->filename[0] == ' ') { ui_error("FILENAME REQUIRED."); break; }
        err = door_save(d, device);
        if (err != BBS_OK) { ui_op_error("SAVE", (u8)err); break; }
        printf("\nDOOR SAVED.\n");
        ui_press_any_key();
        return TRUE;
      }

      case 'C': return FALSE;

      default: ui_beep(); break;
    }
  }
}

/* ------------------------------------------------------------------ */
/* CREATE / EDIT / DELETE                                               */
/* ------------------------------------------------------------------ */

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
  rec.device = device;        /* default to the door device; editable below */
  /* Nothing is written until SAVE in the edit screen. */
  doors_edit_record(&rec, device);
}

static void doors_do_edit(u8 device)
{
  door_record_t door;
  if (doors_browse("SELECT DOOR TO EDIT", device, &door) != BBS_OK) return;
  doors_edit_record(&door, device);
}

static void doors_do_delete(u8 device)
{
  door_record_t door;
  bbs_err_t err;
  char prompt[24];

  if (doors_browse("SELECT DOOR TO DELETE", device, &door) != BBS_OK) return;

  sprintf(prompt, "DELETE DOOR %u?", (unsigned)door.id);
  if (!ui_confirm(prompt)) return;

  err = door_delete(door.id, device);
  if (err != BBS_OK) { ui_op_error("DELETE", (u8)err); return; }
  printf("DOOR DELETED.\n");
  ui_press_any_key();
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                   */
/* ------------------------------------------------------------------ */
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
      case 'L': doors_browse(NULL, device, NULL); break;
      case 'C': doors_do_create(device); break;
      case 'E': doors_do_edit(device);   break;
      case 'D': doors_do_delete(device); break;
      case 'Q': return;
    }
  }
}
