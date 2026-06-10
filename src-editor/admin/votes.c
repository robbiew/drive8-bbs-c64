/**
 * CONFIGURE — Votes Admin Module
 *
 * List, create, view tally, open/close, and delete polls.
 * Option labels are not stored; options are numbered 1–N.
 */

#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "bbs/votes.h"
#include "bbs/err.h"
#include "ui/ui.h"
#include "admin/admin.h"

#define VOTES_PER_PAGE 4

/* -------------------------------------------------------------------------
 * Page buffer
 * ---------------------------------------------------------------------- */
static vote_record_t s_vpage[VOTES_PER_PAGE];
static u8 s_vpage_count;

static void vpage_load(u8 page, u8 device)
{
  u8 i;
  s_vpage_count = 0;
  for (i = 0; i < VOTES_PER_PAGE; i++) {
    u8 idx = (u8)(page * VOTES_PER_PAGE + i + 1);
    if (vote_by_index(idx, &s_vpage[i], device) == BBS_OK) {
      s_vpage_count++;
    } else {
      break;
    }
  }
}

static void vpage_show(u8 page, u8 total)
{
  u8 i;
  ui_screen_header("VOTE/POLL MGMT");
  printf(" #  OPTS  ST  QUESTION\n");
  for (i = 0; i < s_vpage_count; i++) {
    char q[17];
    u8 j;
    for (j = 0; j < 16; j++) {
      char c = s_vpage[i].question[j];
      q[j] = (c == 0) ? ' ' : c;
    }
    q[16] = '\0';
    printf(" %u  %u    %s  %-16s\n",
      (unsigned)(i + 1),
      (unsigned)s_vpage[i].option_count,
      s_vpage[i].active ? "ON" : "--",
      q);
  }
  printf("\n");
  printf("PAGE %u  TOTAL %u\n", (unsigned)(page + 1), (unsigned)total);
}

/* -------------------------------------------------------------------------
 * Pick a vote
 * ---------------------------------------------------------------------- */
static bbs_err_t votes_pick(const char *prompt, u8 device, vote_record_t *out)
{
  u8 total = vote_count(device);
  u8 total_pages, page = 0;

  if (total == 0) {
    ui_error("NO POLLS DEFINED.");
    return BBS_ENOTFOUND;
  }

  total_pages = (u8)((total + VOTES_PER_PAGE - 1) / VOTES_PER_PAGE);

  for (;;) {
    char ch;
    vpage_load(page, device);
    vpage_show(page, total);
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
      if (sel <= s_vpage_count) {
        *out = s_vpage[sel - 1];
        return BBS_OK;
      }
    }
  }
}

/* -------------------------------------------------------------------------
 * LIST
 * ---------------------------------------------------------------------- */
static void votes_do_list(u8 device)
{
  u8 total = vote_count(device);
  u8 total_pages, page = 0;

  if (total == 0) {
    ui_error("NO POLLS DEFINED.");
    return;
  }

  total_pages = (u8)((total + VOTES_PER_PAGE - 1) / VOTES_PER_PAGE);

  for (;;) {
    char ch;
    vpage_load(page, device);
    vpage_show(page, total);
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
static void votes_do_create(u8 device)
{
  char question[25];
  char ch;
  int len;
  u8 new_id = 0, opt_count;
  bbs_err_t err;

  ui_screen_header("CREATE POLL");

  /* Question */
  printf("QUESTION (MAX 24): ");
  len = ui_read_line(question, 24, UI_CASE_MIXED);
  printf("\n");
  if (len == 0) { ui_error("QUESTION CANNOT BE EMPTY."); return; }
  question[len] = '\0';

  /* Option count */
  printf("# OF OPTIONS (2-6): ");
  ch = getchar(); printf("\n");
  if (ch < '2' || ch > '6') { ui_error("OPTIONS MUST BE 2-6."); return; }
  opt_count = (u8)(ch - '0');

  /* Confirm */
  ui_screen_header("CONFIRM CREATE POLL");
  printf("QUESTION: %s\n", question);
  printf("OPTIONS:  %u\n", (unsigned)opt_count);
  printf("\n");
  ui_hotkey_label('Y', "CREATE");
  ui_hotkey_label('N', "CANCEL");
  printf("\n\nCMD?:");
  ch = (char)toupper((unsigned char)getch());
  printf("\n");
  if (ch != 'Y') { printf("CANCELLED.\n"); return; }

  err = vote_create(question, device, &new_id);
  if (err != BBS_OK) {
    ui_op_error("CREATE", (u8)err); return;
  }

  /* Set option count and activate */
  {
    vote_record_t rec;
    if (vote_by_id(new_id, &rec, device) == BBS_OK) {
      rec.option_count = opt_count;
      rec.active = 1;
      vote_save(&rec, device);
    }
  }

  printf("\nPOLL CREATED (ID=%u).\n", (unsigned)new_id);
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * TALLY — show vote counts and optionally toggle open/close
 * ---------------------------------------------------------------------- */
static void votes_do_tally(u8 device)
{
  vote_record_t v;
  char ch;
  u8 i;

  if (votes_pick("SELECT POLL TO VIEW", device, &v) != BBS_OK)
    return;

  ui_screen_header("POLL TALLY");
  printf("Q: %s\n", v.question);
  printf("STATUS: %s\n\n", v.active ? "OPEN" : "CLOSED");
  for (i = 0; i < v.option_count && i < 5; i++) {
    printf("OPTION %u: %u VOTES\n", (unsigned)(i + 1), (unsigned)v.options[i]);
  }
  printf("\n");
  ui_hotkey_label('T', "TOGGLE");
  ui_hotkey_label('Q', "BACK");
  printf("\n\nCMD?:");
  ch = (char)toupper((unsigned char)getch());
  printf("\n");
  if (ch == 'T') {
    bbs_err_t err;
    v.active = v.active ? 0 : 1;
    err = vote_save(&v, device);
    if (err != BBS_OK) { ui_op_error("SAVE", (u8)err); return; }
    printf("POLL IS NOW %s.\n", v.active ? "OPEN" : "CLOSED");
    ui_press_any_key();
  }
}

/* -------------------------------------------------------------------------
 * DELETE
 * ---------------------------------------------------------------------- */
static void votes_do_delete(u8 device)
{
  vote_record_t v;
  char prompt[40];
  bbs_err_t err;

  if (votes_pick("SELECT POLL TO DELETE", device, &v) != BBS_OK)
    return;

  sprintf(prompt, "DELETE POLL %u?", (unsigned)v.id);
  if (!ui_confirm(prompt)) return;

  err = vote_delete(v.id, device);
  if (err != BBS_OK) {
    ui_op_error("DELETE", (u8)err); return;
  }

  printf("\nPOLL DELETED.\n");
  ui_press_any_key();
}

/* -------------------------------------------------------------------------
 * Public entry point
 * ---------------------------------------------------------------------- */
void admin_votes_menu(u8 device)
{
  static const ui_menu_item_t items[] = {
    { 'L', "LIST POLLS"  },
    { 'C', "CREATE POLL" },
    { 'T', "TALLY/TOGGLE"},
    { 'D', "DELETE POLL" },
    { 'Q', "BACK"        },
  };

  for (;;) {
    char ch;
    ui_menu_display("VOTE MGMT", items, 5);
    ch = ui_menu_input("CHOICE:", "LCTDQ");
    switch (ch) {
      case 'L': votes_do_list(device);   break;
      case 'C': votes_do_create(device); break;
      case 'T': votes_do_tally(device);  break;
      case 'D': votes_do_delete(device); break;
      case 'Q': return;
    }
  }
}
