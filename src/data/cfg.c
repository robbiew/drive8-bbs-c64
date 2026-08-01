/**
 * TURBO/64 BBS — Configuration Module (Implementation)
 *
 * Parses key=value lines from "config" sequential file.
 */

#include "bbs/cfg.h"
#include "bbs/err.h"
#include "bbs/config.h"
#include "bbs/drives.h"
#include "bbs/hal/disk.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef T64_BOOT_OVERLAY
#include <c64/kernalio.h>
#endif

/* Global config instance */
cfg_t bbs_cfg;

/**
 * cfg_set_defaults()
 *
 * Initialize with compile-time defaults.
 */
/* cfg_set_defaults .. cfg_parse_device_spec and cfg_apply .. cfg_init are
 * boot-only: they run once from main()'s cfg_init() call and are never used
 * again, so they live in the ovl_boot overlay (freed after boot).  bbs_cfg
 * (the live config), cfg_send_drive_init (called before every disk op), and
 * the editor-path cfg_save/cfg_format stay resident. */
#ifdef T64_BOOT_OVERLAY
#pragma code(boot_code)
#pragma data(boot_data)
#endif
static void cfg_set_defaults(void) {
  strcpy(bbs_cfg.bbs_name, "TURBO/64 BBS");
  strcpy(bbs_cfg.bbs_city, "COMMODORE 64");
  strcpy(bbs_cfg.sysop_name, "SYSTEM");
  strncpy(bbs_cfg.bbs_id, "T64BBS", 8);
  bbs_cfg.bbs_id[8] = '\0';

  bbs_cfg.new_user_level = 1;
  bbs_cfg.new_user_credits = 0;
  bbs_cfg.min_call_time = 5;
  bbs_cfg.max_call_time = 60;
  bbs_cfg.call_time_limit = 3600;
  bbs_cfg.idle_timeout_mins = 3;

  bbs_cfg.device_system = T64_DRIVE_SYSTEM;
  bbs_cfg.drive_system = CFG_DRIVE_DEFAULT;
  bbs_cfg.init_system[0] = '\0';
  bbs_cfg.device_msgs = T64_DRIVE_MSGS;
  bbs_cfg.drive_msgs = CFG_DRIVE_DEFAULT;
  bbs_cfg.init_msgs[0] = '\0';
  bbs_cfg.device_files = T64_DRIVE_FILES;
  bbs_cfg.drive_files = CFG_DRIVE_DEFAULT;
  bbs_cfg.init_files[0] = '\0';
  bbs_cfg.device_doors = T64_DRIVE_DOORS;
  bbs_cfg.drive_doors = CFG_DRIVE_DEFAULT;
  bbs_cfg.init_doors[0] = '\0';

  strcpy(bbs_cfg.modem_init, "ATZ");
  bbs_cfg.baud_rate = 9600;
  bbs_cfg.modem_timeout = 255;  /* Max 255 seconds */
  bbs_cfg.modem_type = MODEM_AUTO;

  bbs_cfg.allow_new_users = TRUE;
  bbs_cfg.allow_uploads = TRUE;
  bbs_cfg.prompt_cursor = FALSE;
  bbs_cfg.petscii_lower_art = TRUE;   /* default: lowercase/mixed-case PETSCII art */

  /* REU is auto-detected at boot; not persisted in config */
  bbs_cfg.reu_enabled = FALSE;
  bbs_cfg.reu_detected_size = 0;
}

/**
 * cfg_parse_line()
 *
 * Split a "key=value" line into key and value buffers.
 * Handles whitespace trimming.
 */
static bool_t cfg_parse_line(const char *line, char *out_key, char *out_value) {
  const char *eq;
  char *p_key;
  const char *p_val;
  int len;

  /* Skip leading whitespace */
  while (*line && (*line == ' ' || *line == '\t')) {
    line++;
  }

  /* Skip comments and empty lines */
  if (!*line || *line == '#' || *line == ';') {
    return FALSE;
  }

  /* Find '=' separator */
  eq = strchr(line, '=');
  if (!eq) {
    return FALSE;
  }

  /* Copy key (trim trailing spaces) */
  len = (int)(eq - line);
  if (len >= CFG_KEY_MAX) {
    len = CFG_KEY_MAX - 1;
  }
  strncpy(out_key, line, len);
  out_key[len] = '\0';

  /* Trim trailing spaces from key */
  p_key = out_key + len - 1;
  while (p_key >= out_key && (*p_key == ' ' || *p_key == '\t')) {
    *p_key = '\0';
    p_key--;
  }

  /* Copy value (skip leading spaces) */
  p_val = (char *)(eq + 1);
  while (*p_val && (*p_val == ' ' || *p_val == '\t')) {
    p_val++;
  }
  len = strlen(p_val);

  /* Trim trailing spaces and newline from value */
  while (len > 0 && (p_val[len - 1] == ' ' || p_val[len - 1] == '\t' ||
                     p_val[len - 1] == '\r' || p_val[len - 1] == '\n')) {
    len--;
  }

  if (len >= CFG_VALUE_MAX) {
    len = CFG_VALUE_MAX - 1;
  }
  strncpy(out_value, p_val, len);
  out_value[len] = '\0';

  return TRUE;
}

bool_t cfg_parse_device_spec(const char *value, u8 *device, u8 *drive,
                             char *init, u8 init_len) {
  const char *p;
  const char *q;

  if (!value || !device || !drive || !init || init_len == 0) {
    return FALSE;
  }

  p = value;
  while (*p == ' ' || *p == '\t') p++;
  if (*p == '\0') return FALSE;

  *device = (u8)atoi(p);
  *drive = CFG_DRIVE_DEFAULT;
  init[0] = '\0';

  q = p;
  while (*q && *q != ';' && *q != '\r' && *q != '\n' && *q != ' ' && *q != '\t') {
    q++;
  }
  if (*q != ';') {
    return TRUE;
  }

  p = q + 1;
  *drive = (u8)atoi(p);
  while (*p && *p != ':' && *p != '\r' && *p != '\n' && *p != ' ' && *p != '\t') {
    p++;
  }
  if (*p != ':') {
    return TRUE;
  }

  p++;
  if (*p == ';') {
    p++;
  }
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '\0') {
    u8 len = (u8)strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
                       p[len - 1] == '\r' || p[len - 1] == '\n')) {
      len--;
    }
    if (len >= init_len) {
      len = (u8)(init_len - 1);
    }
    strncpy(init, p, len);
    init[len] = '\0';
  }

  return TRUE;
}

#ifdef T64_BOOT_OVERLAY
#pragma code(code)
#pragma data(data)
#endif
void cfg_format_device_spec(char *buf, u8 device, u8 drive, const char *init) {
  if (!buf) {
    return;
  }
  if (init && init[0] != '\0') {
    sprintf(buf, "%u;%u:;%s", (unsigned)device, (unsigned)drive, init);
  } else if (drive != CFG_DRIVE_DEFAULT) {
    sprintf(buf, "%u;%u:", (unsigned)device, (unsigned)drive);
  } else {
    sprintf(buf, "%u", (unsigned)device);
  }
}

bbs_err_t cfg_send_drive_init(u8 device, const char *init) {
  if (!init || init[0] == '\0') {
    return BBS_OK;
  }
  return disk_cmd(device, init);
}

/**
 * cfg_apply()
 *
 * Take a parsed key=value pair and update bbs_cfg.
 */
#ifdef T64_BOOT_OVERLAY
#pragma code(boot_code)
#pragma data(boot_data)
#endif
static void cfg_apply(const char *key, const char *value) {
  if (strcmp(key, "BBS_NAME") == 0) {
    strncpy(bbs_cfg.bbs_name, value, sizeof(bbs_cfg.bbs_name) - 1);
    bbs_cfg.bbs_name[sizeof(bbs_cfg.bbs_name) - 1] = '\0';
  } else if (strcmp(key, "BBS_CITY") == 0) {
    strncpy(bbs_cfg.bbs_city, value, sizeof(bbs_cfg.bbs_city) - 1);
    bbs_cfg.bbs_city[sizeof(bbs_cfg.bbs_city) - 1] = '\0';
  } else if (strcmp(key, "SYSOP_NAME") == 0) {
    strncpy(bbs_cfg.sysop_name, value, sizeof(bbs_cfg.sysop_name) - 1);
    bbs_cfg.sysop_name[sizeof(bbs_cfg.sysop_name) - 1] = '\0';
  } else if (strcmp(key, "BBS_ID") == 0) {
    strncpy(bbs_cfg.bbs_id, value, 8);
    bbs_cfg.bbs_id[8] = '\0';
    /* uppercase-force: PETSCII-safe */
    {
        u8 i;
        for (i = 0; i < 8 && bbs_cfg.bbs_id[i]; i++) {
            char c = bbs_cfg.bbs_id[i];
            if (c >= 'a' && c <= 'z') bbs_cfg.bbs_id[i] = c - 32;
        }
    }
  } else if (strcmp(key, "NEW_USER_LEVEL") == 0) {
    bbs_cfg.new_user_level = (u8)atoi(value);
    /* A hand-edited CONFIG must not seed accounts the user_unpack clamp
     * would demote to DELETED; the editor UI caps this at CO as well. */
    if (bbs_cfg.new_user_level > CFG_ACCESS_CO)
      bbs_cfg.new_user_level = CFG_ACCESS_NEW;
  } else if (strcmp(key, "MIN_CALL_TIME") == 0) {
    bbs_cfg.min_call_time = (u8)atoi(value);
  } else if (strcmp(key, "MAX_CALL_TIME") == 0) {
    bbs_cfg.max_call_time = (u8)atoi(value);
  } else if (strcmp(key, "IDLE_TIMEOUT") == 0) {
    bbs_cfg.idle_timeout_mins = (u8)atoi(value);
  } else if (strcmp(key, "DEV_SYSTEM") == 0) {
    if (cfg_parse_device_spec(value, &bbs_cfg.device_system,
                              &bbs_cfg.drive_system, bbs_cfg.init_system,
                              (u8)sizeof(bbs_cfg.init_system)) == FALSE) {
      bbs_cfg.device_system = (u8)atoi(value);
      bbs_cfg.drive_system = CFG_DRIVE_DEFAULT;
      bbs_cfg.init_system[0] = '\0';
    }
  } else if (strcmp(key, "DEV_MSGS") == 0) {
    if (cfg_parse_device_spec(value, &bbs_cfg.device_msgs,
                              &bbs_cfg.drive_msgs, bbs_cfg.init_msgs,
                              (u8)sizeof(bbs_cfg.init_msgs)) == FALSE) {
      bbs_cfg.device_msgs = (u8)atoi(value);
      bbs_cfg.drive_msgs = CFG_DRIVE_DEFAULT;
      bbs_cfg.init_msgs[0] = '\0';
    }
  } else if (strcmp(key, "DEV_FILES") == 0) {
    if (cfg_parse_device_spec(value, &bbs_cfg.device_files,
                              &bbs_cfg.drive_files, bbs_cfg.init_files,
                              (u8)sizeof(bbs_cfg.init_files)) == FALSE) {
      bbs_cfg.device_files = (u8)atoi(value);
      bbs_cfg.drive_files = CFG_DRIVE_DEFAULT;
      bbs_cfg.init_files[0] = '\0';
    }
  } else if (strcmp(key, "DEV_DOORS") == 0) {
    if (cfg_parse_device_spec(value, &bbs_cfg.device_doors,
                              &bbs_cfg.drive_doors, bbs_cfg.init_doors,
                              (u8)sizeof(bbs_cfg.init_doors)) == FALSE) {
      bbs_cfg.device_doors = (u8)atoi(value);
      bbs_cfg.drive_doors = CFG_DRIVE_DEFAULT;
      bbs_cfg.init_doors[0] = '\0';
    }
  } else if (strcmp(key, "MODEM_INIT") == 0) {
    strncpy(bbs_cfg.modem_init, value, sizeof(bbs_cfg.modem_init) - 1);
    bbs_cfg.modem_init[sizeof(bbs_cfg.modem_init) - 1] = '\0';
  } else if (strcmp(key, "BAUD") == 0 || strcmp(key, "BAUD_RATE") == 0) {
    bbs_cfg.baud_rate = (u16)atoi(value);
  } else if (strcmp(key, "MODEM_TIMEOUT") == 0) {
    bbs_cfg.modem_timeout = (u8)atoi(value);
  } else if (strcmp(key, "MODEM_TYPE") == 0) {
    if (strcmp(value, "VICE") == 0)      bbs_cfg.modem_type = MODEM_VICE;
    else if (strcmp(value, "U64") == 0)  bbs_cfg.modem_type = MODEM_U64;
    else                                 bbs_cfg.modem_type = MODEM_AUTO;
  } else if (strcmp(key, "ALLOW_NEW_USERS") == 0) {
    bbs_cfg.allow_new_users = (atoi(value) != 0) ? TRUE : FALSE;
  } else if (strcmp(key, "ALLOW_UPLOADS") == 0) {
    bbs_cfg.allow_uploads = (atoi(value) != 0) ? TRUE : FALSE;
  } else if (strcmp(key, "PROMPT_CURSOR") == 0) {
    bbs_cfg.prompt_cursor = (atoi(value) != 0) ? TRUE : FALSE;
  } else if (strcmp(key, "PETSCII_LOWER_ART") == 0) {
    bbs_cfg.petscii_lower_art = (atoi(value) != 0) ? TRUE : FALSE;
  }
}

/**
 * cfg_load_impl()
 *
 * Read and parse the config file from disk into bbs_cfg.  Boot-only: lives in
 * the ovl_boot overlay; reached solely through the resident cfg_init() wrapper.
 */
static bbs_err_t cfg_load_impl(void) {
  bbs_err_t err;
  i16 nread;
  char cfg_line[96];
  char cfg_key[CFG_KEY_MAX];
  char cfg_value[CFG_VALUE_MAX];

  /* Start with defaults */
  cfg_set_defaults();

  /* Read CONFIG from the device we were loaded FROM, not from the compile-time
   * default. $BA is the KERNAL's current device, set by the LOAD that brought
   * this program in, so a BBS booted from device 10 reads its config from
   * device 10. Without this the default (T64_DRIVE_SYSTEM, normally 8) wins and
   * a BBS running off any other device silently reads someone else's config and
   * then fails to find its own USR LOG. cfg_init() already loads the OVL_BOOT
   * overlay from $BA; this makes the config agree with it.
   *
   * Values below 8 mean tape or a bus device that cannot hold files, so the
   * compile-time default is kept in that case.
   *
   * Partition: drive_system is still cfg_set_defaults()' value (0) here, so no
   * CP is sent and CONFIG is read from whatever partition the drive powers up
   * on. That is unavoidable — the file cannot say which partition it lives on
   * until it has been read. CONFIG must therefore live on that partition. */
  {
    u8 boot_dev = *(volatile u8 *)0xBA;
    if (boot_dev >= 8) {
      bbs_cfg.device_system = boot_dev;
    }
  }
  err = disk_open(bbs_cfg.device_system, bbs_cfg.drive_system, "CONFIG", DISK_READ);
  if (err != BBS_OK) {
    /* File not found; use defaults */
    return BBS_ENOTFOUND;
  }

  /* Read and parse lines */
  while ((nread = disk_gets(cfg_line, sizeof(cfg_line))) > 0) {
    if (cfg_parse_line(cfg_line, cfg_key, cfg_value)) {
      cfg_apply(cfg_key, cfg_value);
    }
  }

  disk_close();
  return BBS_OK;
}

#ifdef T64_BOOT_OVERLAY
#pragma code(code)
#pragma data(data)
#endif

/**
 * cfg_init()
 *
 * Resident entry point for config load.  In the BOOT build it pulls the
 * ovl_boot overlay into the shared $9700 region before running the boot-only
 * parse code, then returns — the overlay is dead weight afterward and the wfc/
 * msgs overlays freely overwrite it.  The overlay loads from the kernal current
 * device ($BA, the disk the BBS booted from) because bbs_cfg.device_system is
 * not populated until cfg_load_impl() runs.  The editor build has no overlay,
 * so this is a thin pass-through.
 */
bbs_err_t cfg_init(void) {
#ifdef T64_BOOT_OVERLAY
  krnio_setnam(P"OVL_BOOT");
  krnio_load(1, (*(volatile u8 *)0xBA), 1);
#endif
  return cfg_load_impl();
}

/* First disk_puts failure across a whole cfg_save pass; later writes are
 * skipped once an error is latched (they would write into a broken file). */
static bbs_err_t s_save_err;

static void cfg_put(const char *line) {
  if (s_save_err == BBS_OK) {
    s_save_err = disk_puts(line);
  }
}

/**
 * cfg_save()
 *
 * Write current bbs_cfg to the "config" sequential file on the given device.
 * Returns BBS_EFULL on DOS 72 (disk full), BBS_EIO on any other write or
 * drive-status error — callers must treat non-BBS_OK as "CONFIG is suspect".
 */
bbs_err_t cfg_save(void) {
  bbs_err_t err;
  char line[96];
  char spec[CFG_VALUE_MAX];
  u8 status;

  /* WHY drive_system rather than a literal 0: partition 0 means "send no CP,
   * use whatever partition the drive is currently on", and the current
   * partition is mutable state. In CONFIGURE, visiting the message-areas menu
   * selects drive_msgs' partition; a config save afterwards would then land on
   * that partition instead of the one it was read from. Writing to
   * drive_system makes the destination deterministic. */
  disk_scratch(bbs_cfg.device_system, bbs_cfg.drive_system, "CONFIG");

  err = disk_open(bbs_cfg.device_system, bbs_cfg.drive_system, "CONFIG",
                  DISK_WRITE);
  if (err != BBS_OK) {
    return err;
  }

  s_save_err = BBS_OK;

  sprintf(line, "BBS_NAME=%s\n",      bbs_cfg.bbs_name);    cfg_put(line);
  sprintf(line, "BBS_CITY=%s\n",      bbs_cfg.bbs_city);    cfg_put(line);
  sprintf(line, "SYSOP_NAME=%s\n",    bbs_cfg.sysop_name);  cfg_put(line);
  sprintf(line, "NEW_USER_LEVEL=%u\n",(unsigned)bbs_cfg.new_user_level);   cfg_put(line);
  sprintf(line, "MIN_CALL_TIME=%u\n", (unsigned)bbs_cfg.min_call_time);    cfg_put(line);
  sprintf(line, "MAX_CALL_TIME=%u\n", (unsigned)bbs_cfg.max_call_time);    cfg_put(line);
  sprintf(line, "IDLE_TIMEOUT=%u\n",  (unsigned)bbs_cfg.idle_timeout_mins); cfg_put(line);
  cfg_format_device_spec(spec, bbs_cfg.device_system, bbs_cfg.drive_system, bbs_cfg.init_system);
  sprintf(line, "DEV_SYSTEM=%s\n",    spec);                                cfg_put(line);
  cfg_format_device_spec(spec, bbs_cfg.device_msgs, bbs_cfg.drive_msgs, bbs_cfg.init_msgs);
  sprintf(line, "DEV_MSGS=%s\n",      spec);                                cfg_put(line);
  cfg_format_device_spec(spec, bbs_cfg.device_files, bbs_cfg.drive_files, bbs_cfg.init_files);
  sprintf(line, "DEV_FILES=%s\n",     spec);                                cfg_put(line);
  cfg_format_device_spec(spec, bbs_cfg.device_doors, bbs_cfg.drive_doors, bbs_cfg.init_doors);
  sprintf(line, "DEV_DOORS=%s\n",     spec);                                cfg_put(line);
  sprintf(line, "MODEM_INIT=%s\n",    bbs_cfg.modem_init);                 cfg_put(line);
  sprintf(line, "BAUD=%u\n",          (unsigned)bbs_cfg.baud_rate);        cfg_put(line);
  sprintf(line, "MODEM_TIMEOUT=%u\n", (unsigned)bbs_cfg.modem_timeout);    cfg_put(line);
  sprintf(line, "MODEM_TYPE=%s\n",
          bbs_cfg.modem_type == MODEM_VICE ? "VICE" :
          bbs_cfg.modem_type == MODEM_U64  ? "U64"  : "AUTO");             cfg_put(line);
  sprintf(line, "ALLOW_NEW_USERS=%u\n",(unsigned)(bbs_cfg.allow_new_users ? 1 : 0));  cfg_put(line);
  sprintf(line, "ALLOW_UPLOADS=%u\n",  (unsigned)(bbs_cfg.allow_uploads   ? 1 : 0));  cfg_put(line);
  sprintf(line, "PROMPT_CURSOR=%u\n",  (unsigned)(bbs_cfg.prompt_cursor    ? 1 : 0));  cfg_put(line);
  sprintf(line, "PETSCII_LOWER_ART=%u\n",(unsigned)(bbs_cfg.petscii_lower_art ? 1 : 0)); cfg_put(line);

  disk_close();

  if (s_save_err != BBS_OK) {
    /* Still read the error channel: maps DISK FULL precisely and clears
     * the drive's latched error before the next command. */
    status = disk_status(bbs_cfg.device_system);
    return (status == 72) ? BBS_EFULL : s_save_err;
  }

  /* KERNAL buffers writes; DOS 72 (DISK FULL) only surfaces on the drive
   * status channel after close. */
  status = disk_status(bbs_cfg.device_system);
  if (status >= 20) {
    return (status == 72) ? BBS_EFULL : BBS_EIO;
  }
  return BBS_OK;
}

/**
 * Not implemented (would require keeping parsed lines in memory).
 * For now, direct access via bbs_cfg struct fields.
 */
const char *cfg_get(const char *key) {
  (void)key;
  return NULL;
}

/**
 * cfg_get_u8() / cfg_get_u16()
 *
 * Helper functions for integer lookups.
 * Not implemented; use bbs_cfg fields directly.
 */
u8 cfg_get_u8(const char *key, u8 default_val) {
  (void)key;
  return default_val;
}

u16 cfg_get_u16(const char *key, u16 default_val) {
  (void)key;
  return default_val;
}
