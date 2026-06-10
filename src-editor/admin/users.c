/* CONFIGURE Admin - User management workflows. */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "bbs/users.h"
#include "bbs/records.h"
#include "bbs/types.h"
#include "bbs/err.h"
#include "ui/ui.h"

#define USERS_PER_PAGE  4

/* Static page buffer — only current page's records loaded at once */
static user_record_t s_page[USERS_PER_PAGE];
static u8 s_page_count;

/* Reusable edit field buffers (static = no stack alloc) */
static char s_edit_handle[16];
static char s_edit_access[3];
static char s_edit_credits[4];
static char s_edit_termmode[2];   /* "0"–"4" */
static char s_edit_termwidth[3];  /* "40" or "80" */
static char s_edit_termrows[3];   /* "24" or "25" */
static char s_edit_clearonmsg[2]; /* "Y" or "N" */

/* Profile edit buffers */
static char s_edit_email[33];
static char s_edit_firstname[17];
static char s_edit_lastname[17];
static char s_edit_location[22];

/* State used by field validators */
static u8 s_edit_device;
static u8 s_edit_user_id;

static const char *validate_handle(const char *val, void *ctx)
{
  u8 found;
  (void)ctx;
  found = user_by_handle(val, s_edit_device);
  if (found != 0 && found != s_edit_user_id)
    return "HANDLE ALREADY IN USE.";
  return 0;
}

static const char *validate_access(const char *val, void *ctx)
{
  (void)ctx;
  if (val[0] < '0' || val[0] > '5' || val[1] != '\0')
    return "INVALID LEVEL (0-5).";
  return 0;
}

static const char *validate_credits(const char *val, void *ctx)
{
  int v, i;
  (void)ctx;
  v = 0;
  for (i = 0; val[i]; i++) {
    if (val[i] < '0' || val[i] > '9') return "DIGITS ONLY.";
    v = v * 10 + (val[i] - '0');
  }
  if (v > 255) return "MAX 255.";
  return 0;
}

static const char *validate_termmode(const char *val, void *ctx)
{
  (void)ctx;
  if (val[0] < '0' || val[0] > '2' || val[1] != '\0')
    return "0=PETSCII 1=ANSI 2=ASCII";
  return 0;
}

static const char *validate_termwidth(const char *val, void *ctx)
{
  (void)ctx;
  if ((val[0]=='4'&&val[1]=='0'&&val[2]=='\0') ||
      (val[0]=='8'&&val[1]=='0'&&val[2]=='\0'))
    return 0;
  return "ENTER 40 OR 80.";
}

static const char *validate_termrows(const char *val, void *ctx)
{
  (void)ctx;
  if ((val[0]=='2'&&val[1]=='4'&&val[2]=='\0') ||
      (val[0]=='2'&&val[1]=='5'&&val[2]=='\0'))
    return 0;
  return "ENTER 24 OR 25.";
}

static const char *validate_clearonmsg(const char *val, void *ctx)
{
  (void)ctx;
  if ((val[0]=='Y'||val[0]=='y'||val[0]=='N'||val[0]=='n') && val[1]=='\0')
    return 0;
  return "ENTER Y OR N.";
}

/* ------------------------------------------------------------------ */
/* Private helpers                                                      */
/* ------------------------------------------------------------------ */

/* Load USERS_PER_PAGE users for the given 0-based page into s_page[]. */
static void page_load(u8 page, u8 device)
{
  u8 start_n = (u8)(page * USERS_PER_PAGE + 1);  /* user_by_index is 1-based */
  u8 i;
  s_page_count = 0;
  for (i = 0; i < USERS_PER_PAGE; i++) {
    if (user_by_index((u8)(start_n + i), &s_page[i], device) != BBS_OK)
      break;
    s_page_count++;
  }
}

/* Display the current page of users.
 * Row format:  " N  HANDLE          LVL CALLS" (pick by line #) */
static void page_show(u8 page, u8 total_count)
{
  u8 total_pages = (u8)((total_count + USERS_PER_PAGE - 1) / USERS_PER_PAGE);
  u8 i;

  ui_screen_header("USERS");
  printf("PAGE %u/%u\n\n", (unsigned)(page + 1), (unsigned)total_pages);
  printf(" #  HANDLE           LVL CALLS\n");

  for (i = 0; i < s_page_count; i++) {
    char h[16];
    u8 j;
    for (j = 0; j < 15; j++) {
      char c = s_page[i].handle[j];
      h[j] = (c == 0 || c == ' ') ? ' ' : c;
    }
    h[15] = '\0';
    printf(" %u  %-15s  %u %4u\n",
      (unsigned)(i + 1),
      h,
      (unsigned)s_page[i].access_level,
      (unsigned)s_page[i].calls);
  }
  printf("\n");
}

/* Let the sysop pick a user from a paged list.
 * Returns BBS_OK with *out populated, or BBS_ENOTFOUND if cancelled. */
static bbs_err_t users_pick(const char *prompt, u8 device, user_record_t *out)
{
  u8 total = user_count(device);
  u8 total_pages;
  u8 page = 0;

  if (total == 0) {
    ui_error("NO USERS IN DATABASE.");
    return BBS_ENOTFOUND;
  }

  total_pages = (u8)((total + USERS_PER_PAGE - 1) / USERS_PER_PAGE);

  for (;;) {
    char ch;
    page_load(page, device);
    page_show(page, total);

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
    if (ch >= '1' && ch <= (char)('0' + s_page_count)) {
      *out = s_page[(u8)(ch - '1')];
      return BBS_OK;
    }
    ui_beep();
  }
}

/* ------------------------------------------------------------------ */
/* Admin operations                                                     */
/* ------------------------------------------------------------------ */

static void users_do_list(u8 device)
{
  u8 total = user_count(device);
  u8 total_pages;
  u8 page = 0;

  if (total == 0) {
    ui_error("NO USERS IN DATABASE.");
    return;
  }

  total_pages = (u8)((total + USERS_PER_PAGE - 1) / USERS_PER_PAGE);

  for (;;) {
    char ch;
    page_load(page, device);
    page_show(page, total);

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

static void users_do_create(u8 device)
{
  user_record_t new_user;
  user_profile_record_t new_prof;
  char handle_buf[16];
  char credits_str[4];
  char ch;
  int len, val, i;
  u8 new_id;
  bbs_err_t err;

  ui_screen_header("CREATE USER");

  /* -- Handle -- */
  printf("HANDLE (MAX 15 CHARS): ");
  len = 0;
  memset(handle_buf, 0, sizeof(handle_buf));
  for (;;) {
    ch = getchar();
    if (ch == 13 || ch == '\n') { printf("\n"); break; }
    if (ch == 20 || ch == 8) {
      if (len > 0) { len--; handle_buf[len] = 0; printf("\x08 \x08"); }
      continue;
    }
    if (ch >= 32 && ch <= 126 && len < 15) {
      ch = (char)toupper((unsigned char)ch);
      handle_buf[len++] = ch;
    }
  }
  if (len == 0) { ui_error("HANDLE CANNOT BE EMPTY."); return; }
  if (user_by_handle(handle_buf, device) != 0) {
    ui_error("HANDLE ALREADY IN USE.");
    return;
  }

  /* -- Access level -- */
  printf("ACCESS LEVEL:\n");
  printf("0=BANNED 1=NEW   2=USER\n");
  printf("3=POWER  4=CO    5=SYSOP\n");
  printf("LEVEL (0-5): ");
  ch = getchar();
  printf("\n");
  if (ch < '0' || ch > '5') { ui_error("INVALID ACCESS LEVEL."); return; }
  s_edit_access[0] = ch;
  s_edit_access[1] = 0;

  /* -- Credits -- */
  printf("CREDITS (0-255): ");
  len = 0;
  memset(credits_str, 0, sizeof(credits_str));
  for (;;) {
    ch = getchar();
    if (ch == 13 || ch == '\n') { printf("\n"); break; }
    if (ch == 20 || ch == 8) {
      if (len > 0) { len--; credits_str[len] = 0; printf("\x08 \x08"); }
      continue;
    }
    if (ch >= '0' && ch <= '9' && len < 3) {
      credits_str[len++] = ch;
    }
  }

  /* -- Terminal mode -- */
  printf("TERM MODE:\n");
  printf("0=PETSCII 1=ANSI 2=ASCII\n");
  printf("MODE (0-2): ");
  ch = getchar();
  printf("\n");
  if (ch < '0' || ch > '2') ch = '0';
  s_edit_termmode[0] = ch;
  s_edit_termmode[1] = '\0';

  /* -- Columns -- */
  printf("COLUMNS (4=40, 8=80): ");
  ch = getchar();
  printf("\n");
  if (ch == '8') { s_edit_termwidth[0]='8'; }
  else           { s_edit_termwidth[0]='4'; }
  s_edit_termwidth[1] = '0';
  s_edit_termwidth[2] = '\0';

  /* -- Rows -- */
  printf("ROWS (4=24, 5=25): ");
  ch = getchar();
  printf("\n");
  if (ch == '5') { s_edit_termrows[1]='5'; }
  else           { s_edit_termrows[1]='4'; }
  s_edit_termrows[0] = '2';
  s_edit_termrows[2] = '\0';

  /* -- Get next available ID -- */
  new_id = user_next_id(device);
  if (new_id == 0) { ui_error("USER DATABASE FULL."); return; }

  /* -- Confirm -- */
  ui_screen_header("CONFIRM CREATE USER");
  printf("HANDLE:  %s\n", handle_buf);
  printf("ACCESS:  %c\n", s_edit_access[0]);
  printf("CREDITS: %s\n", (credits_str[0] ? credits_str : "0"));
  printf("TERM:    %c  COLS: %s  ROWS: %s\n",
         s_edit_termmode[0], s_edit_termwidth, s_edit_termrows);
  printf("NEW ID:  %u\n\n", (unsigned)new_id);
  ui_hotkey_label('Y', "CREATE");
  ui_hotkey_label('N', "CANCEL");
  printf("\n\nCMD?:");
  ch = (char)toupper((unsigned char)getch());
  printf("\n");
  if (ch != 'Y') { printf("CANCELLED.\n"); return; }

  /* -- Build and save user record -- */
  memset(&new_user, 0, sizeof(new_user));
  new_user.id = new_id;
  strncpy(new_user.handle, handle_buf, 15);
  new_user.access_level = (u8)(s_edit_access[0] - '0');
  val = 0;
  for (i = 0; credits_str[i]; i++) val = val * 10 + (credits_str[i] - '0');
  if (val > 255) val = 255;
  new_user.credit_balance = (u8)val;
  new_user.calls = 1;
  new_user.term_mode  = (u8)(s_edit_termmode[0] - '0');
  val = (s_edit_termwidth[0] - '0') * 10 + (s_edit_termwidth[1] - '0');
  new_user.term_width = (u8)val;
  val = (s_edit_termrows[0] - '0') * 10 + (s_edit_termrows[1] - '0');
  new_user.term_rows  = (u8)val;

  /* Default password: "PASS" (sysop can reset via [R] option) */
  user_hash_password("PASS", new_user.password);

  err = user_save(&new_user, device);
  if (err != BBS_OK) {
    ui_op_error("CREATE", (u8)err); return;
  }

  /* -- Create empty profile record -- */
  memset(&new_prof, 0, sizeof(new_prof));
  new_prof.id = new_id;
  err = user_profile_save(&new_prof, device);
  if (err != BBS_OK)
    printf("PROFILE INIT FAILED (CODE %u).\n", (unsigned)err);

  printf("\nUSER CREATED (ID=%u).\n", (unsigned)new_id);
  printf("DEFAULT PASSWORD: PASS\n");
  ui_press_any_key();
}

static void users_do_edit(u8 device)
{
  user_record_t user;
  user_profile_record_t prof;
  ui_edit_field_t fields[8];
  bbs_err_t err;
  int result, val, i;
  char title[32];

  if (users_pick("SELECT USER TO EDIT", device, &user) != BBS_OK)
    return;

  s_edit_device  = device;
  s_edit_user_id = user.id;

  for (;;) {
    char ch;
    ui_screen_header("EDIT USER");
    printf("USER: %s (ID=%u)\n\n", user.handle, (unsigned)user.id);
    putchar(' '); ui_hotkey_label('1', "ACCOUNT (HANDLE/ACCESS/CREDITS)"); printf("\n");
    putchar(' '); ui_hotkey_label('2', "PROFILE (NAME/EMAIL/TERM/LOC)");   printf("\n");
    putchar(' '); ui_hotkey_label('3', "RESET PASSWORD");                  printf("\n");
    putchar(' '); ui_hotkey_label('B', "BACK");                            printf("\n\n");
    ch = ui_menu_input("CHOICE:", "123B");
    if (ch == 'B') return;

    if (ch == '1') {
      /* -- Account fields (sysop-managed) -- */
      strncpy(s_edit_handle, user.handle, 15);
      s_edit_handle[15] = '\0';
      for (i = 14; i >= 0 && s_edit_handle[i] == ' '; i--)
        s_edit_handle[i] = '\0';

      s_edit_access[0] = (char)('0' + user.access_level);
      s_edit_access[1] = '\0';

      {
        int c = (int)user.credit_balance;
        if (c >= 100) {
          s_edit_credits[0] = (char)('0' + c / 100);
          s_edit_credits[1] = (char)('0' + (c % 100) / 10);
          s_edit_credits[2] = (char)('0' + c % 10);
          s_edit_credits[3] = '\0';
        } else if (c >= 10) {
          s_edit_credits[0] = (char)('0' + c / 10);
          s_edit_credits[1] = (char)('0' + c % 10);
          s_edit_credits[2] = '\0';
        } else {
          s_edit_credits[0] = (char)('0' + c);
          s_edit_credits[1] = '\0';
        }
      }

      memset(fields, 0, sizeof(fields));   /* zero is_toggle + all members up front */
      fields[0].label = "HANDLE";    fields[0].value = s_edit_handle;
      fields[0].max_len = 15;         fields[0].current_len = (int)strlen(s_edit_handle);
      fields[0].dirty = 0;            fields[0].validate = validate_handle;
      fields[0].validate_ctx = NULL;

      fields[1].label = "ACCESSLVL"; fields[1].value = s_edit_access;
      fields[1].max_len = 1;          fields[1].current_len = 1;
      fields[1].dirty = 0;            fields[1].validate = validate_access;
      fields[1].validate_ctx = NULL;

      fields[2].label = "CREDITS";   fields[2].value = s_edit_credits;
      fields[2].max_len = 3;          fields[2].current_len = (int)strlen(s_edit_credits);
      fields[2].dirty = 0;            fields[2].validate = validate_credits;
      fields[2].validate_ctx = NULL;

      fields[0].case_mode = UI_CASE_UPPER;  /* HANDLE — identity key */
      fields[1].case_mode = UI_CASE_UPPER;  /* ACCESSLVL */
      fields[2].case_mode = UI_CASE_UPPER;  /* CREDITS */

      sprintf(title, "ACCOUNT: %s", user.handle);
      result = ui_edit_form(title, fields, 3);
      if (result == -1) continue;

      strncpy(user.handle, s_edit_handle, 15);
      user.handle[15] = '\0';
      user.access_level = (u8)(s_edit_access[0] - '0');
      val = 0;
      for (i = 0; s_edit_credits[i]; i++) {
        if (s_edit_credits[i] >= '0' && s_edit_credits[i] <= '9')
          val = val * 10 + (s_edit_credits[i] - '0');
      }
      if (val > 255) val = 255;
      user.credit_balance = (u8)val;

      err = user_save(&user, device);
      if (err != BBS_OK) { ui_op_error("SAVE", (u8)err); continue; }
      printf("\nACCOUNT SAVED.\n");
      ui_press_any_key();

    } else if (ch == '2') {
      /* -- Profile fields (user preferences + personal info) -- */
      err = user_profile_by_id(user.id, &prof, device);
      if (err != BBS_OK) {
        memset(&prof, 0, sizeof(prof));
        prof.id = user.id;
      }

      strncpy(s_edit_email,     prof.email,     32); s_edit_email[32]     = '\0';
      strncpy(s_edit_firstname, prof.firstname, 16); s_edit_firstname[16] = '\0';
      strncpy(s_edit_lastname,  prof.lastname,  16); s_edit_lastname[16]  = '\0';
      strncpy(s_edit_location,  prof.location,  21); s_edit_location[21]  = '\0';

      s_edit_termmode[0] = (char)('0' + (user.term_mode < 5 ? user.term_mode : 0));
      s_edit_termmode[1] = '\0';
      s_edit_termwidth[0] = (user.term_width == 80) ? '8' : '4';
      s_edit_termwidth[1] = '0';
      s_edit_termwidth[2] = '\0';
      s_edit_termrows[0] = '2';
      s_edit_termrows[1] = (user.term_rows == 25) ? '5' : '4';
      s_edit_termrows[2] = '\0';

      s_edit_clearonmsg[0] = (user.flags & USER_F_CLEAR_ON_MSG) ? 'Y' : 'N';
      s_edit_clearonmsg[1] = '\0';

      memset(fields, 0, sizeof(fields));   /* zero is_toggle + all members up front */
      fields[0].label = "EMAIL";      fields[0].value = s_edit_email;
      fields[0].max_len = 32;          fields[0].current_len = (int)strlen(s_edit_email);
      fields[0].dirty = 0;             fields[0].validate = NULL;
      fields[0].validate_ctx = NULL;

      fields[1].label = "FIRST NAME"; fields[1].value = s_edit_firstname;
      fields[1].max_len = 15;          fields[1].current_len = (int)strlen(s_edit_firstname);
      fields[1].dirty = 0;             fields[1].validate = NULL;
      fields[1].validate_ctx = NULL;

      fields[2].label = "LAST NAME";  fields[2].value = s_edit_lastname;
      fields[2].max_len = 15;          fields[2].current_len = (int)strlen(s_edit_lastname);
      fields[2].dirty = 0;             fields[2].validate = NULL;
      fields[2].validate_ctx = NULL;

      fields[3].label = "LOCATION";   fields[3].value = s_edit_location;
      fields[3].max_len = 20;          fields[3].current_len = (int)strlen(s_edit_location);
      fields[3].dirty = 0;             fields[3].validate = NULL;
      fields[3].validate_ctx = NULL;

      fields[4].label = "TERM (0-4)"; fields[4].value = s_edit_termmode;
      fields[4].max_len = 1;           fields[4].current_len = 1;
      fields[4].dirty = 0;             fields[4].validate = validate_termmode;
      fields[4].validate_ctx = NULL;

      fields[5].label = "COLS";       fields[5].value = s_edit_termwidth;
      fields[5].max_len = 2;           fields[5].current_len = 2;
      fields[5].dirty = 0;             fields[5].validate = validate_termwidth;
      fields[5].validate_ctx = NULL;

      fields[6].label = "ROWS";       fields[6].value = s_edit_termrows;
      fields[6].max_len = 2;           fields[6].current_len = 2;
      fields[6].dirty = 0;             fields[6].validate = validate_termrows;
      fields[6].validate_ctx = NULL;

      fields[7].label = "CLR ON MSG";  fields[7].value = s_edit_clearonmsg;
      fields[7].max_len = 1;            fields[7].current_len = 1;
      fields[7].dirty = 0;              fields[7].validate = validate_clearonmsg;
      fields[7].validate_ctx = NULL;

      fields[0].case_mode = UI_CASE_MIXED;  /* EMAIL */
      fields[1].case_mode = UI_CASE_MIXED;  /* FIRST NAME */
      fields[2].case_mode = UI_CASE_MIXED;  /* LAST NAME */
      fields[3].case_mode = UI_CASE_MIXED;  /* LOCATION */
      fields[4].case_mode = UI_CASE_UPPER;  /* TERM (0-4) */
      fields[5].case_mode = UI_CASE_UPPER;  /* COLS */
      fields[6].case_mode = UI_CASE_UPPER;  /* ROWS */
      fields[7].case_mode = UI_CASE_UPPER;  /* CLR ON MSG */

      sprintf(title, "PROFILE: %s", user.handle);
      result = ui_edit_form(title, fields, 8);
      if (result == -1) continue;

      strncpy(prof.email,     s_edit_email,     32); prof.email[31]     = '\0';
      strncpy(prof.firstname, s_edit_firstname, 16); prof.firstname[15] = '\0';
      strncpy(prof.lastname,  s_edit_lastname,  16); prof.lastname[15]  = '\0';
      strncpy(prof.location,  s_edit_location,  21); prof.location[20]  = '\0';

      user.term_mode  = (u8)(s_edit_termmode[0] - '0');
      val = (s_edit_termwidth[0] - '0') * 10 + (s_edit_termwidth[1] - '0');
      user.term_width = (u8)val;
      val = (s_edit_termrows[0] - '0') * 10 + (s_edit_termrows[1] - '0');
      user.term_rows  = (u8)val;
      if (s_edit_clearonmsg[0] == 'Y' || s_edit_clearonmsg[0] == 'y')
        user.flags |=  USER_F_CLEAR_ON_MSG;
      else
        user.flags &= (u8)~USER_F_CLEAR_ON_MSG;

      err = user_profile_save(&prof, device);
      if (err != BBS_OK) { ui_op_error("PROFILE SAVE", (u8)err); continue; }
      err = user_save(&user, device);
      if (err != BBS_OK) { ui_op_error("SAVE", (u8)err); continue; }
      printf("\nPROFILE SAVED.\n");
      ui_press_any_key();

    } else {
      /* -- Reset password -- */
      char pw[5];
      int pwlen = 0;
      char pwch;

      ui_screen_header("RESET PASSWORD");
      printf("USER: %s\n\n", user.handle);
      printf("NEW PW (MAX 4 CHARS):\n");

      memset(pw, 0, sizeof(pw));
      for (;;) {
        pwch = getchar();
        if (pwch == 13 || pwch == '\n') { printf("\n"); break; }
        if ((pwch == 20 || pwch == 8) && pwlen > 0) { pwlen--; pw[pwlen] = 0; printf("\x08 \x08"); continue; }
        if (pwch >= 32 && pwch <= 126 && pwlen < 4) { pw[pwlen++] = (char)toupper((unsigned char)pwch); printf("*"); }
      }

      if (pwlen == 0) { ui_error("PASSWORD CANNOT BE EMPTY."); continue; }

      ui_hotkey_label('Y', "CONFIRM");
      ui_hotkey_label('N', "CANCEL");
      printf("\n\nCMD?:");
      pwch = (char)toupper((unsigned char)getch());
      printf("\n");
      if (pwch != 'Y') continue;

      err = user_reset_password(user.id, pw, device);
      if (err != BBS_OK) { ui_op_error("RESET PW", (u8)err); continue; }
      printf("\nPASSWORD RESET.\n");
      ui_press_any_key();
    }
  }
}


static void users_do_delete(u8 device)
{
  user_record_t user;
  char prompt[40];
  bbs_err_t err;

  if (users_pick("SELECT USER TO DELETE", device, &user) != BBS_OK)
    return;

  if (user.id == 1) {
    ui_error("CANNOT DELETE SYSOP (ID=1).");
    return;
  }

  sprintf(prompt, "DELETE %s (ID=%u)?", user.handle, (unsigned)user.id);
  if (!ui_confirm(prompt)) return;

  err = user_delete(user.id, device);
  if (err != BBS_OK) {
    ui_op_error("DELETE", (u8)err); return;
  }

  printf("\nDELETED: %s\n", user.handle);
  ui_press_any_key();
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                   */
/* ------------------------------------------------------------------ */

void admin_users_menu(u8 device)
{
  static const ui_menu_item_t items[] = {
    { 'L', "LIST USERS"  },
    { 'C', "CREATE USER" },
    { 'E', "EDIT USER"   },
    { 'D', "DELETE USER" },
    { 'B', "BACK"        },
  };

  for (;;) {
    char ch;
    ui_menu_display("USER MANAGEMENT", items, 5);
    ch = ui_menu_input("CHOICE:", "LCEDB");
    switch (ch) {
      case 'L': users_do_list(device);   break;
      case 'C': users_do_create(device); break;
      case 'E': users_do_edit(device);   break;
      case 'D': users_do_delete(device); break;
      case 'B': return;
      default:  break;
    }
  }
}
