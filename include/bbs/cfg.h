/**
 * TURBO/64 BBS — Configuration Module
 *
 * Loads and manages the "setup" file — a key=value text file on device DEV_SYSTEM.
 * Provides compile-time defaults and runtime overrides.
 */

#ifndef INCLUDE_BBS_CFG_H
#define INCLUDE_BBS_CFG_H

#include "types.h"
#include "err.h"

/** Configuration value buffer size (for key and value) */
#define CFG_KEY_MAX     32
#define CFG_VALUE_MAX   64
#define CFG_INIT_MAX    16

/** Modem carrier-detection mode.
 *  AUTO: probe for U64 UCI support at boot; use U64 if present, otherwise VICE.
 *  VICE: force AT-string mode (VICE/tcpser — DSR line is meaningless).
 *  U64 : force the DSR hardware line (real Ultimate 64 SwiftLink). */
typedef enum {
  MODEM_AUTO = 0,
  MODEM_VICE = 1,
  MODEM_U64  = 2
} modem_type_t;

/** Configuration structure — runtime BBS settings */
typedef struct {
  /* BBS identity */
  char bbs_name[24];
  char bbs_city[24];
  char sysop_name[20];
  char bbs_id[9];               /* T/64 network node identifier (8 chars + NUL) */

  /* Access & timing */
  u8 new_user_level;        /* Access level for new users */
  u8 new_user_credits;      /* Credit balance for new users */
  u8 min_call_time;          /* Minimum call duration (minutes) */
  u8 max_call_time;          /* Maximum call duration (minutes) */
  u8 idle_timeout_mins;      /* Keyboard idle timeout (minutes); 0 = disabled */
  u16 call_time_limit;       /* Per-access-level time limit (for future expansion) */

  /* Device assignments */
  u8 device_system;          /* CBM device for system files (default 8) */
  u8 drive_system;           /* Logical drive / partition for system files */
  char init_system[CFG_INIT_MAX]; /* DOS init command for system device */
  u8 device_msgs;            /* CBM device for message bases (default 8) */
  u8 drive_msgs;             /* Logical drive / partition for message bases */
  char init_msgs[CFG_INIT_MAX]; /* DOS init command for message device */
  u8 device_files;           /* CBM device for file bases (default 9) */
  u8 drive_files;            /* Logical drive / partition for file bases */
  char init_files[CFG_INIT_MAX]; /* DOS init command for file device */
  u8 device_doors;           /* CBM device for door programs (default 10) */
  u8 drive_doors;            /* Logical drive / partition for door programs */
  char init_doors[CFG_INIT_MAX]; /* DOS init command for door device */

  /* Modem / network */
  char modem_init[20];       /* AT init string */
  u16 baud_rate;             /* Initial baud (300–2400; SwiftLink higher) */
  u8 modem_timeout;          /* Seconds before no-carrier disconnect */
  modem_type_t modem_type;   /* carrier-detection mode (AUTO/VICE/U64) */

  /* Flags */
  bool_t allow_new_users;    /* Enable new user registration */
  bool_t allow_uploads;      /* Enable file uploads */
  bool_t prompt_cursor;      /* Animated color-cycling block cursor at the menu prompt */

  /* RAM Expansion Unit (REU) — auto-detected at boot, not persisted */
  bool_t reu_enabled;        /* TRUE if REU was detected and is active */
  u16 reu_detected_size;     /* REU size in KB (0=none, 128, 512) */
} cfg_t;

/** Global config structure */
extern cfg_t bbs_cfg;

/**
 * cfg_init()
 *
 * Load configuration from "setup" file on device DEV_SYSTEM.
 * Falls back to compile-time defaults if file missing.
 * 
 * Returns:
 *   BBS_OK         — loaded successfully
 *   BBS_ENOTFOUND     — setup file not found (using defaults)
 *   BBS_EREAD      — disk read error
 */
bbs_err_t cfg_init(void);

/**
 * cfg_save()
 *
 * Write current bbs_cfg to "setup" file on device DEV_SYSTEM.
 * Overwrites any existing file.
 *
 * Returns:
 *   BBS_OK      — saved successfully
 *   BBS_EFULL   — disk full (DOS error 72)
 *   BBS_EIO     — write or drive-status error
 */
bbs_err_t cfg_save(u8 device);

/**
 * cfg_parse_device_spec()
 *
 * Parse a device specification string.
 * Accepts legacy "8" as well as extended "8;0:;i0:" format.
 */
bool_t cfg_parse_device_spec(const char *value, u8 *device, u8 *drive,
                             char *init, u8 init_len);

/**
 * cfg_format_device_spec()
 *
 * Format a device specification string for display or storage.
 */
void cfg_format_device_spec(char *buf, u8 device, u8 drive, const char *init);

/**
 * cfg_send_drive_init()
 *
 * Send a stored DOS init command to a device before disk access.
 */
bbs_err_t cfg_send_drive_init(u8 device, const char *init);


/**
 * cfg_get()
 *
 * Look up a key in the parsed config (or get default).
 * 
 * Returns pointer to value string, or NULL if not found.
 */
const char *cfg_get(const char *key);

/**
 * cfg_get_u8() / cfg_get_u16()
 *
 * Convenience functions to parse config values as integers.
 * Return 0 if not found or parse error.
 */
u8  cfg_get_u8(const char *key, u8 default_val);
u16 cfg_get_u16(const char *key, u16 default_val);

#endif /* INCLUDE_BBS_CFG_H */
