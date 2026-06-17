/* CONFIGURE Admin - Message area management workflows. */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bbs/boards.h"
#include "bbs/records.h"
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/users.h"
#include "bbs/hal/disk.h"
#include "bbs/cfg.h"
#include "bbs/messages.h"
#include "bbs/config.h"
#include "ui/ui.h"

#define BOARDS_PER_PAGE  4

static board_dir_record_t s_bpage[BOARDS_PER_PAGE];
static u8 s_bpage_count;

/* Edit field buffers (edit flow) */
static char s_edit_net_tag[9];
static char s_edit_subop_name[17];
static u16  s_edit_subop_id;
static u8   s_edit_max_msgs;
static u8   s_edit_max_age;

/* Shared board editor; returns TRUE if saved, FALSE if cancelled. */
static bool_t boards_edit_record(board_dir_record_t *board, u8 device);

/* ------------------------------------------------------------------ */
/* Private helpers                                                      */
/* ------------------------------------------------------------------ */

static void bpage_load(u8 page, u8 device)
{
  u8 start_n = (u8)(page * BOARDS_PER_PAGE + 1);
  u8 i;
  s_bpage_count = 0;
  for (i = 0; i < BOARDS_PER_PAGE; i++) {
    if (board_by_index((u8)(start_n + i), &s_bpage[i], device) != BBS_OK)
      break;
    s_bpage_count++;
  }
}

static void bpage_show(u8 page, u8 total)
{
  u8 total_pages = (u8)((total + BOARDS_PER_PAGE - 1) / BOARDS_PER_PAGE);
  u8 i;

  ui_screen_header("MSG AREAS");
  printf("PAGE %u/%u\n\n", (unsigned)(page + 1), (unsigned)total_pages);
  printf(" #  TITLE            RD WR MSG FLG\n");

  for (i = 0; i < s_bpage_count; i++) {
    char t[17];
    char fl[4];
    u8 f = s_bpage[i].flags;
    u8 j;
    for (j = 0; j < 16; j++) {
      char c = s_bpage[i].title[j];
      t[j] = (c == 0) ? ' ' : c;
    }
    t[16] = '\0';
    fl[0] = (f & BOARD_F_ANON)   ? 'A' : '-';
    fl[1] = (f & BOARD_F_NET)    ? 'N' : '-';
    fl[2] = (f & BOARD_F_STICKY) ? 'S' : '-';
    fl[3] = '\0';
    printf(" %u  %-16s  %u  %u %3u %s\n",
      (unsigned)(i + 1),
      t,
      (unsigned)s_bpage[i].read_level,
      (unsigned)s_bpage[i].write_level,
      (unsigned)s_bpage[i].msg_count,
      fl);
  }
  printf("\n");
}

static bbs_err_t boards_pick(const char *prompt, u8 device, board_dir_record_t *out)
{
  u8 total = board_count(device);
  u8 total_pages;
  u8 page = 0;

  if (total == 0) {
    ui_error("NO MESSAGE AREAS DEFINED.");
    return BBS_ENOTFOUND;
  }

  total_pages = (u8)((total + BOARDS_PER_PAGE - 1) / BOARDS_PER_PAGE);

  for (;;) {
    char ch;
    bpage_load(page, device);
    bpage_show(page, total);

    if (page > 0)                     ui_hotkey_label('P', "PREV");
    if (page < (u8)(total_pages - 1)) ui_hotkey_label('N', "NEXT");
    ui_hotkey_label('Q', "BACK");
    printf("\n\n%s\n", prompt);
    printf("# TO SELECT: ");

    ch = (char)toupper((unsigned char)getch());
    printf("\n");

    if (ch == 'Q') return BBS_ENOTFOUND;
    if (ch == 'N' && page < (u8)(total_pages - 1)) { page++; continue; }
    if (ch == 'P' && page > 0)                      { page--; continue; }
    if (ch >= '1' && ch <= (char)('0' + s_bpage_count)) {
      *out = s_bpage[(u8)(ch - '1')];
      return BBS_OK;
    }
    ui_beep();
  }
}

/* ------------------------------------------------------------------ */
/* Admin operations                                                     */
/* ------------------------------------------------------------------ */

static void boards_do_list(u8 device)
{
  u8 total = board_count(device);
  u8 total_pages;
  u8 page = 0;

  if (total == 0) {
    ui_error("NO MESSAGE AREAS DEFINED.");
    return;
  }

  total_pages = (u8)((total + BOARDS_PER_PAGE - 1) / BOARDS_PER_PAGE);

  for (;;) {
    char ch;
    bpage_load(page, device);
    bpage_show(page, total);

    if (page > 0)                     ui_hotkey_label('P', "PREV");
    if (page < (u8)(total_pages - 1)) ui_hotkey_label('N', "NEXT");
    ui_hotkey_label('Q', "DONE");
    printf("\n\nCMD?:");

    ch = (char)toupper((unsigned char)getch());
    printf("\n");

    if (ch == 'Q') return;
    if (ch == 'N' && page < (u8)(total_pages - 1)) page++;
    else if (ch == 'P' && page > 0)                page--;
    else ui_beep();
  }
}

static void boards_do_create(u8 device)
{
  char title[17];
  int len;
  u8 new_id = 0;
  board_dir_record_t board;
  bbs_err_t err;

  ui_screen_header("CREATE MSG AREA");

  printf("TITLE (MAX 16 CHARS): ");
  len = ui_read_line(title, 16, UI_CASE_MIXED);
  printf("\n");
  if (len == 0) { ui_error("TITLE CANNOT BE EMPTY."); return; }

  err = board_create(title, 0, 0, device, &new_id);
  if (err != BBS_OK) { ui_op_error("CREATE", (u8)err); return; }

  if (board_by_id(new_id, &board, device) != BBS_OK) {
    board_delete(new_id, device);   /* best-effort cleanup of the half-made board */
    ui_op_error("CREATE", (u8)BBS_ENOTFOUND); return;
  }

  /* Open the shared editor on the new board; discard it if cancelled. */
  if (!boards_edit_record(&board, device)) {
    board_delete(new_id, device);
  }
}

static void boards_show_edit_screen(const board_dir_record_t *b)
{
  char buf[48];
  char t[17];
  u8 j;

  sprintf(buf, "EDIT MSG AREA #%u", (unsigned)b->id);
  ui_screen_header(buf);

  for (j = 0; j < 16; j++) t[j] = (b->title[j] == 0) ? ' ' : b->title[j];
  t[16] = '\0';
  for (j = 15; j > 0 && (t[j] == ' ' || t[j] == '\0'); j--) t[j] = '\0';

  ui_edit_label('T', "TITLE");    printf("%s\n", t);
  ui_edit_label('L', "LIST ORD"); printf("%u\n", (unsigned)b->display_order);
  ui_edit_label('R', "READ");     printf("%u\n", (unsigned)b->read_level);
  ui_edit_label('W', "WRITE");    printf("%u\n", (unsigned)b->write_level);
  ui_edit_label('A', "ANON");     printf("%c\n", (b->flags & BOARD_F_ANON) ? 'Y' : 'N');
  ui_edit_label('N', "NET");      printf("%c\n", (b->flags & BOARD_F_NET)  ? 'Y' : 'N');

  ui_edit_label('O', "SUBOP");
  if (s_edit_subop_id != 0)
    printf("%s (ID %u)\n", s_edit_subop_name, (unsigned)s_edit_subop_id);
  else
    printf("(NONE)\n");

  if (s_edit_subop_id != 0) {
    putchar(' '); ui_print_hotkey('X');
    textcolor(COLOR_LT_GREEN); printf(" CLEAR SUBOP\n");
    textcolor(COLOR_WHITE);
  }

  ui_edit_label('M', "MAX MSG");
  if (s_edit_max_msgs == 0) printf("DEFAULT\n");
  else                      printf("%u\n", (unsigned)s_edit_max_msgs);

  ui_edit_label('D', "MAX DAYS");
  if (s_edit_max_age == 0) printf("OFF\n");
  else                     printf("%u\n", (unsigned)s_edit_max_age);

  if (b->flags & BOARD_F_NET) {
    ui_edit_label('G', "NET TAG");
    printf("%s\n", s_edit_net_tag[0] ? s_edit_net_tag : "(NONE)");
  }

  printf("\n ");
  ui_print_hotkey('S'); textcolor(COLOR_LT_GREEN); printf(" SAVE   ");
  ui_print_hotkey('C'); textcolor(COLOR_LT_GREEN); printf(" CANCEL\n");
  textcolor(COLOR_WHITE);
  printf("\n");
}

static bool_t boards_edit_record(board_dir_record_t *board, u8 device)
{
  s_edit_subop_id = board->subop_id;
  s_edit_max_msgs = board->max_msgs;
  s_edit_max_age  = board->max_age_days;
  if (board->subop_id != 0) {
    user_record_t urec;
    if (user_by_id((u8)board->subop_id, &urec, device) == BBS_OK) {
      strncpy(s_edit_subop_name, urec.handle, 16);
      s_edit_subop_name[16] = '\0';
    } else {
      strcpy(s_edit_subop_name, "?");
    }
  } else {
    s_edit_subop_name[0] = '\0';
  }
  {
    u8 j;
    for (j = 0; j < 8; j++) s_edit_net_tag[j] = board->net_area_tag[j];
    s_edit_net_tag[8] = '\0';
    for (j = 7; j > 0 && (s_edit_net_tag[j] == ' ' || s_edit_net_tag[j] == '\0'); j--)
      s_edit_net_tag[j] = '\0';
  }

  for (;;) {
    char ch;
    boards_show_edit_screen(board);
    textcolor(COLOR_WHITE);
    printf("CMD?:");
    ch = (char)toupper((unsigned char)getch());
    printf("\n");

    switch (ch) {
      case 'T': {
        char newt[17];
        int len;
        printf("TITLE (16 CHARS MAX): ");
        len = ui_read_line(newt, 16, UI_CASE_MIXED);
        printf("\n");
        if (len > 0) {
          int i;
          strncpy(board->title, newt, 16);
          for (i = len; i < 16; i++) board->title[i] = ' ';
        }
        break;
      }

      case 'R': {
        char lc;
        printf("READ LEVEL (0-5): ");
        lc = getch(); putchar(lc); printf("\n");
        if (lc >= '0' && lc <= '5') board->read_level = (u8)(lc - '0');
        else ui_error("INVALID LEVEL.");
        break;
      }

      case 'W': {
        char lc;
        printf("WRITE LEVEL (0-5): ");
        lc = getch(); putchar(lc); printf("\n");
        if (lc >= '0' && lc <= '5') board->write_level = (u8)(lc - '0');
        else ui_error("INVALID LEVEL.");
        break;
      }

      case 'A': board->flags ^= BOARD_F_ANON; break;
      case 'N': board->flags ^= BOARD_F_NET;  break;

      case 'O': {
        char handle[17];
        int hlen;
        u8 uid;
        printf("SUBOP HANDLE: ");
        hlen = ui_read_line(handle, 16, UI_CASE_UPPER);
        if (hlen == 0) break;
        uid = user_by_handle(handle, device);
        if (uid == 0) { ui_error("HANDLE NOT FOUND."); break; }
        s_edit_subop_id = (u16)uid;
        strncpy(s_edit_subop_name, handle, 16);
        s_edit_subop_name[16] = '\0';
        board->subop_id = (u16)uid;
        break;
      }

      case 'X':
        s_edit_subop_id = 0;
        s_edit_subop_name[0] = '\0';
        board->subop_id = 0;
        break;

      case 'G': {
        printf("NET TAG (8 CHARS, EG T64.GENL): ");
        ui_read_line(s_edit_net_tag, 8, UI_CASE_UPPER);
        break;
      }

      case 'M': {
        int v = ui_read_num("MAX MSGS (0=DEFAULT, 1-255): ");
        s_edit_max_msgs = (u8)(v < 0 ? 0 : v);
        board->max_msgs = s_edit_max_msgs;
        break;
      }

      case 'D': {
        int v = ui_read_num("MAX DAYS (0=DISABLED, 1-255): ");
        s_edit_max_age = (u8)(v < 0 ? 0 : v);
        board->max_age_days = s_edit_max_age;
        break;
      }

      case 'L': {
        int v = ui_read_num("ORDER (1-255): ");
        if (v < 0) break;                     /* blank = keep current */
        if (v < 1) v = 1;
        board->display_order = (u8)v;
        break;
      }

      case 'S': {
        bbs_err_t err;
        err = board_save(board, device);
        if (err != BBS_OK) { ui_op_error("SAVE", (u8)err); break; }
        if (board->flags & BOARD_F_NET) {
          bbs_err_t nerr;
          nerr = board_set_net_area(board->id, s_edit_net_tag[0] ? s_edit_net_tag : NULL, device);
          if (nerr != BBS_OK) { ui_op_error("NET TAG", (u8)nerr); }
        } else {
          board_set_net_area(board->id, NULL, device);
        }
        printf("\nBOARD SAVED.\n");
        ui_press_any_key();
        return TRUE;
      }

      case 'C': return FALSE;

      default:
        ui_beep();
        break;
    }
  }
}

static void boards_do_edit(u8 device)
{
  board_dir_record_t board;

  if (boards_pick("SELECT AREA TO EDIT", device, &board) != BBS_OK)
    return;

  boards_edit_record(&board, device);
}

static void boards_do_delete(u8 device)
{
  board_dir_record_t board;
  char prompt[40];
  char fname[16];
  bbs_err_t err;

  if (boards_pick("SELECT AREA TO DELETE", device, &board) != BBS_OK)
    return;

  sprintf(prompt, "DELETE AREA %u: %.12s?", (unsigned)board.id, board.title);
  if (!ui_confirm(prompt)) return;

  err = board_delete(board.id, device);
  if (err != BBS_OK) {
    ui_op_error("DELETE", (u8)err); return;
  }

  sprintf(fname, "B%u.IDX", (unsigned)board.id);
  disk_scratch(device, bbs_cfg.drive_msgs, fname);
  sprintf(fname, "B%u.TXT", (unsigned)board.id);
  disk_scratch(device, bbs_cfg.drive_msgs, fname);

  printf("\nDELETED: %.16s\n", board.title);
  ui_press_any_key();
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                   */
/* ------------------------------------------------------------------ */

void admin_messages_menu(u8 device)
{
  static const ui_menu_item_t items[] = {
    { 'L', "LIST AREAS"   },
    { 'C', "CREATE AREA"  },
    { 'E', "EDIT AREA"    },
    { 'D', "DELETE AREA"  },
    { 'B', "BACK"         },
  };

  for (;;) {
    char ch;
    ui_menu_display("MESSAGE AREAS", items, 5);
    ch = ui_menu_input("CHOICE:", "LCEDB");
    switch (ch) {
      case 'L': boards_do_list(device);   break;
      case 'C': boards_do_create(device); break;
      case 'E': boards_do_edit(device);   break;
      case 'D': boards_do_delete(device); break;
      case 'B': return;
      default:  break;
    }
  }
}
