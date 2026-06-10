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

/* " <key> LABEL   : " — reverse hotkey, green padded label; leaves color white
 * for the caller to print the value. Fixed 8-wide label keeps colons aligned. */
static void edit_row_label(char key, const char *label)
{
  putchar(' ');
  ui_print_hotkey(key);
  textcolor(COLOR_LT_GREEN);
  printf(" %-8s: ", label);
  textcolor(COLOR_WHITE);
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

  edit_row_label('T', "TITLE");    printf("%s\n", t);
  edit_row_label('L', "LIST ORD"); printf("%u\n", (unsigned)b->display_order);
  edit_row_label('R', "READ");     printf("%u\n", (unsigned)b->read_level);
  edit_row_label('W', "WRITE");    printf("%u\n", (unsigned)b->write_level);
  edit_row_label('A', "ANON");     printf("%c\n", (b->flags & BOARD_F_ANON) ? 'Y' : 'N');
  edit_row_label('N', "NET");      printf("%c\n", (b->flags & BOARD_F_NET)  ? 'Y' : 'N');

  edit_row_label('O', "SUBOP");
  if (s_edit_subop_id != 0)
    printf("%s (ID %u)\n", s_edit_subop_name, (unsigned)s_edit_subop_id);
  else
    printf("(NONE)\n");

  if (s_edit_subop_id != 0) {
    putchar(' '); ui_print_hotkey('X');
    textcolor(COLOR_LT_GREEN); printf(" CLEAR SUBOP\n");
    textcolor(COLOR_WHITE);
  }

  edit_row_label('M', "MAX MSG");
  if (s_edit_max_msgs == 0) printf("DEFAULT\n");
  else                      printf("%u\n", (unsigned)s_edit_max_msgs);

  edit_row_label('D', "MAX DAYS");
  if (s_edit_max_age == 0) printf("OFF\n");
  else                     printf("%u\n", (unsigned)s_edit_max_age);

  if (b->flags & BOARD_F_NET) {
    edit_row_label('G', "NET TAG");
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
        lc = getchar(); printf("\n");
        if (lc >= '0' && lc <= '5') board->read_level = (u8)(lc - '0');
        else ui_error("INVALID LEVEL.");
        break;
      }

      case 'W': {
        char lc;
        printf("WRITE LEVEL (0-5): ");
        lc = getchar(); printf("\n");
        if (lc >= '0' && lc <= '5') board->write_level = (u8)(lc - '0');
        else ui_error("INVALID LEVEL.");
        break;
      }

      case 'A': board->flags ^= BOARD_F_ANON; break;
      case 'N': board->flags ^= BOARD_F_NET;  break;

      case 'O': {
        char handle[17];
        int hlen = 0;
        u8 uid;
        memset(handle, 0, sizeof(handle));
        printf("SUBOP HANDLE: ");
        for (;;) {
          char hc = getchar();
          if (hc == 13 || hc == '\n') { printf("\n"); break; }
          if ((hc == 20 || hc == 8) && hlen > 0) { hlen--; handle[hlen] = 0; printf("\x08 \x08"); continue; }
          if (hc >= 32 && hc <= 126 && hlen < 16) {
            hc = (char)toupper((unsigned char)hc);
            handle[hlen++] = hc;
          }
        }
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
        int tlen = 0;
        memset(s_edit_net_tag, 0, sizeof(s_edit_net_tag));
        printf("NET TAG (8 CHARS, EG T64.GENL): ");
        for (;;) {
          char tc = getchar();
          if (tc == 13 || tc == '\n') { printf("\n"); break; }
          if ((tc == 20 || tc == 8) && tlen > 0) { tlen--; s_edit_net_tag[tlen] = 0; printf("\x08 \x08"); continue; }
          if (tc >= 32 && tc <= 126 && tlen < 8) {
            tc = (char)toupper((unsigned char)tc);
            s_edit_net_tag[tlen++] = tc;
          }
        }
        break;
      }

      case 'M': {
        char mc[4];
        int mlen = 0, mv = 0, mi;
        memset(mc, 0, sizeof(mc));
        printf("MAX MSGS (0=DEFAULT, 1-255): ");
        for (;;) {
          char mc2 = getchar();
          if (mc2 == 13 || mc2 == '\n') { printf("\n"); break; }
          if ((mc2 == 20 || mc2 == 8) && mlen > 0) { mlen--; mc[mlen] = 0; printf("\x08 \x08"); continue; }
          if (mc2 >= '0' && mc2 <= '9' && mlen < 3) mc[mlen++] = mc2;
        }
        for (mi = 0; mc[mi]; mi++) mv = mv * 10 + (mc[mi] - '0');
        s_edit_max_msgs = (u8)(mv > 255 ? 255 : mv);
        board->max_msgs = s_edit_max_msgs;
        break;
      }

      case 'D': {
        char dc[4];
        int dlen = 0, dv = 0, di;
        memset(dc, 0, sizeof(dc));
        printf("MAX DAYS (0=DISABLED, 1-255): ");
        for (;;) {
          char dc2 = getchar();
          if (dc2 == 13 || dc2 == '\n') { printf("\n"); break; }
          if ((dc2 == 20 || dc2 == 8) && dlen > 0) { dlen--; dc[dlen] = 0; printf("\x08 \x08"); continue; }
          if (dc2 >= '0' && dc2 <= '9' && dlen < 3) dc[dlen++] = dc2;
        }
        for (di = 0; dc[di]; di++) dv = dv * 10 + (dc[di] - '0');
        s_edit_max_age = (u8)(dv > 255 ? 255 : dv);
        board->max_age_days = s_edit_max_age;
        break;
      }

      case 'L': {
        char oc[4];
        int olen = 0, ov = 0, oi;
        memset(oc, 0, sizeof(oc));
        printf("ORDER (1-255): ");
        for (;;) {
          char c2 = getchar();
          if (c2 == 13 || c2 == '\n') { printf("\n"); break; }
          if ((c2 == 20 || c2 == 8) && olen > 0) { olen--; oc[olen] = 0; printf("\x08 \x08"); continue; }
          if (c2 >= '0' && c2 <= '9' && olen < 3) oc[olen++] = c2;
        }
        if (olen == 0) break;                 /* blank = keep current */
        for (oi = 0; oc[oi]; oi++) ov = ov * 10 + (oc[oi] - '0');
        if (ov < 1) ov = 1;
        if (ov > 255) ov = 255;
        board->display_order = (u8)ov;
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

static void boards_do_maint(u8 device)
{
  board_dir_record_t board;
  char ch;
  bbs_err_t err;

  if (boards_pick("SELECT AREA FOR MAINTENANCE", device, &board) != BBS_OK)
    return;

  for (;;) {
    u16 total_msgs = 0, deleted_msgs = 0;
    char buf[48];

    msg_index_stats(board.id, device, &total_msgs, &deleted_msgs);

    sprintf(buf, "MAINTENANCE: BOARD #%u", (unsigned)board.id);
    ui_screen_header(buf);

    {
      char title[17];
      u8 j;
      for (j = 0; j < 16; j++) title[j] = (board.title[j] == 0) ? ' ' : board.title[j];
      title[16] = '\0';
      for (j = 15; j > 0 && (title[j] == ' ' || title[j] == '\0'); j--) title[j] = '\0';
      printf(" BOARD    : %s\n", title);
    }
    printf(" TOTAL    : %u  DELETED: %u\n",
           (unsigned)total_msgs, (unsigned)deleted_msgs);
    if (total_msgs > 0) {
      u8 waste_pct = (u8)((u16)(deleted_msgs * 100) / total_msgs);
      printf(" WASTE    : %u%%\n", (unsigned)waste_pct);
      if (waste_pct >= 25) printf(" [!] COMPACT RECOMMENDED\n");
    }
    printf("\n ");
    ui_hotkey_label('C', "COMPACT");
    ui_hotkey_label('P', "PRUNE");
    ui_hotkey_label('Q', "QUIT");
    printf("\n\nCMD?:");
    ch = (char)toupper((unsigned char)getch());
    printf("\n");

    if (ch == 'Q') break;

    if (ch == 'C') {
      printf("COMPACTING...\n");
      err = msg_compact(board.id, device);
      if (err == BBS_ENOTIMPL) {
        ui_error("COMPACT NOT IMPLEMENTED.");
      } else if (err != BBS_OK) {
        ui_op_error("COMPACT", (u8)err);
      } else {
        printf("COMPACT DONE.\n");
        ui_press_any_key();
        board_by_id(board.id, &board, device);
      }
      continue;
    }

    if (ch == 'P') {
      printf("PRUNING...\n");
      err = msg_prune_quantity(board.id, device);
      if (err != BBS_OK && err != BBS_ENOTFOUND) ui_op_error("PRUNE-QTY", (u8)err);
      printf("PRUNE DONE.\n");
      ui_press_any_key();
      board_by_id(board.id, &board, device);
      continue;
    }

    ui_beep();
  }
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
    { 'M', "MAINTENANCE"  },
    { 'B', "BACK"         },
  };

  for (;;) {
    char ch;
    ui_menu_display("MESSAGE AREAS", items, 6);
    ch = ui_menu_input("CHOICE:", "LCEDMB");
    switch (ch) {
      case 'L': boards_do_list(device);   break;
      case 'C': boards_do_create(device); break;
      case 'E': boards_do_edit(device);   break;
      case 'D': boards_do_delete(device); break;
      case 'M': boards_do_maint(device);  break;
      case 'B': return;
      default:  break;
    }
  }
}
