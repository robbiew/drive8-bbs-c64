/* CONFIGURE Setup Module - Initialize BBS files and user database.
 *
 * Provides functions to create/initialize BBS data files (USR LOG REL file,
 * etc.) and populate initial sysop account. Called during first-time BBS setup.
 */
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include "bbs/version.h"
#include "bbs/config.h"
#include "bbs/types.h"
#include "bbs/rel.h"
#include "bbs/records.h"
#include "bbs/err.h"
#include "bbs/cfg.h"
#include "bbs/hal/disk.h"
#include "bbs/users.h"
#include "bbs/access.h"
#include "ui/ui.h"

/* Initialize USR LOG REL file with sysop account.
 *
 * Uses rel_open / rel_position / rel_write / rel_close — the same path
 * that the BBS runtime (user_save) uses.  rel_reset() is called first to
 * clear the single-open guard in case any previous operation left it set.
 *
 * The P command in rel_position uses PRINT#-style (CHKOUT + CHROUT), which
 * is the reliable canonical method on 1581/U64 CBM DOS.
 */
bbs_err_t setup_create_user_database(u8 device) {
  bbs_err_t err;
  rel_handle_t rh;
  user_record_t user;
  u8 buf[RECORD_SIZE_USER];
  u8 i;
  u16 rec;

  printf("CREATING USER DATABASE...\n");

  (void)disk_scratch(device, 0, "USR LOG");

  memset(&user, 0, sizeof(user));
  user.id = 1;
  strncpy(user.handle, "SYSOP", sizeof(user.handle) - 1);
  user.handle[sizeof(user.handle) - 1] = '\0';
  user_hash_password(1, "PASS", user.password);
  user.access_level   = 5;
  user.calls          = 1;
  user.credit_balance = 100;
  user.flags          = USER_F_CLEAR_ON_MSG;  /* default sysop: clear-screen on */

  rel_reset();
  err = rel_open(device, 0, "USR LOG", RECORD_SIZE_USER, &rh);
  if (err != BBS_OK && err != BBS_ENOTFOUND) {
    printf("OPEN FAILED (CODE %u)\n", (unsigned)err);
    return err;
  }
  if (err == BBS_ENOTFOUND) {
    rel_reset();
    err = rel_open(device, 0, "USR LOG", RECORD_SIZE_USER, &rh);
    if (err != BBS_OK) {
      printf("CREATE FAILED (CODE %u)\n", (unsigned)err);
      return err;
    }
  }

  memset(buf, 0, RECORD_SIZE_USER);
  buf[0] = user.id;
  for (i = 0; i < 15; i++) {
    char c = user.handle[i];
    buf[1 + i] = (u8)(c == 0 ? ' ' : c);
  }
  for (i = 0; i < 4; i++) buf[16 + i] = (u8)user.password[i];
  buf[20] = user.access_level;
  buf[21] = user.credit_balance;
  buf[22] = (u8)(user.calls & 0xFF);
  buf[23] = (u8)((user.calls >> 8) & 0xFF);
  buf[24] = user.downloads;
  buf[25] = user.uploads;
  buf[26] = user.term_mode;
  buf[27] = user.term_width;
  buf[28] = user.term_rows;
  buf[29] = user.flags;

  err = rel_write(rh, (const void *)buf, RECORD_SIZE_USER);
  if (err != BBS_OK) {
    rel_close(rh);
    printf("WRITE FAILED (CODE %u)\n", (unsigned)err);
    return err;
  }

  memset(buf, 0, RECORD_SIZE_USER);
  printf("ALLOCATING %u SLOTS", (unsigned)USERS_MAX);
  for (rec = 2; rec <= USERS_MAX; rec++) {
    err = rel_write(rh, (const void *)buf, RECORD_SIZE_USER);
    if (err != BBS_OK) {
      rel_close(rh);
      printf("\nFAILED AT SLOT %u (CODE %u)\n", (unsigned)rec, (unsigned)err);
      return err;
    }
    if ((rec % 20) == 0) printf(".");
  }
  printf("\n");

  rel_close(rh);

  {
    u8 dstat = disk_status(device);
    if (dstat >= 20 && dstat != 62) {
      printf("DRIVE ERR %u - %s\n", (unsigned)dstat, disk_errmsg);
      return BBS_EIO;
    }
  }

  printf("DONE - %u SLOTS\n", (unsigned)USERS_MAX);
  printf("SYSOP / PASS (LEVEL 5)\n");

  return BBS_OK;
}

/* Initialize USR PROF REL file with empty profile records. */
bbs_err_t setup_create_user_profiles(u8 device) {
  rel_handle_t rh;
  bbs_err_t err;
  u8 buf[RECORD_SIZE_USER_PROFILE];
  u16 rec;

  printf("CREATING USR PROF...\n");

  (void)disk_scratch(device, 0, "USR PROF");
  rel_reset();

  err = rel_open(device, 0, "USR PROF", RECORD_SIZE_USER_PROFILE, &rh);
  if (err != BBS_OK && err != BBS_ENOTFOUND) {
    printf("OPEN FAILED (CODE %u)\n", (unsigned)err);
    return err;
  }
  if (err == BBS_ENOTFOUND) {
    rel_reset();
    err = rel_open(device, 0, "USR PROF", RECORD_SIZE_USER_PROFILE, &rh);
    if (err != BBS_OK) {
      printf("CREATE FAILED (CODE %u)\n", (unsigned)err);
      return err;
    }
  }

  memset(buf, 0, RECORD_SIZE_USER_PROFILE);
  printf("ALLOCATING %u SLOTS", (unsigned)USERS_MAX);
  for (rec = 1; rec <= USERS_MAX; rec++) {
    if (rec == 1) {
      /* Sysop placeholder profile */
      user_profile_record_t sysop_prof;
      memset(&sysop_prof, 0, sizeof(sysop_prof));
      sysop_prof.id = 1;
      strncpy(sysop_prof.email,     "SYSOP@YOURBBS.COM", sizeof(sysop_prof.email) - 1);
      strncpy(sysop_prof.firstname, "SYSOP",             sizeof(sysop_prof.firstname) - 1);
      strncpy(sysop_prof.lastname,  "ADMIN",             sizeof(sysop_prof.lastname) - 1);
      strncpy(sysop_prof.location,  "YOUR CITY",         sizeof(sysop_prof.location) - 1);
      err = rel_write(rh, (const void *)&sysop_prof, RECORD_SIZE_USER_PROFILE);
    } else {
      err = rel_write(rh, (const void *)buf, RECORD_SIZE_USER_PROFILE);
    }
    if (err != BBS_OK) {
      rel_close(rh);
      printf("\nFAILED AT SLOT %u (CODE %u)\n", (unsigned)rec, (unsigned)err);
      return err;
    }
    if ((rec % 20) == 0) printf(".");
  }
  printf("\n");

  rel_close(rh);

  {
    u8 dstat = disk_status(device);
    if (dstat >= 20 && dstat != 62) {
      printf("DRIVE ERR %u - %s\n", (unsigned)dstat, disk_errmsg);
      return BBS_EIO;
    }
  }

  printf("DONE - %u SLOTS\n", (unsigned)USERS_MAX);
  return BBS_OK;
}

/* Create the "access" SEQ file with the 6 default access levels.
 * Uses the shared data-layer defaults + save so the on-disk format matches
 * exactly what the BBS runtime and the editor read back. */
bbs_err_t setup_create_access_levels(u8 device) {
  access_level_t levels[ACCESS_LEVEL_COUNT];

  printf("CREATING ACCESS LEVELS...\n");
  access_levels_defaults(levels);
  return access_levels_save(levels, device);
}
