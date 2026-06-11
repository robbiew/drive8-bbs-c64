#ifndef BBS_CONFIG_H
#define BBS_CONFIG_H

/* -----------------------------------------------------------------------
 * Compile-time defaults — all values can be overridden at runtime via the
 * SETUP file on disk (edited with the CONFIGURE SysOp tool).
 * ----------------------------------------------------------------------- */

/* Device numbers for each storage area (CBM IEC, 8–11) */
#define CFG_DEV_SYSTEM      8   /* Boot PRG, SETUP, help files          */
#define CFG_DEV_MSGS        8   /* Bulletin boards and private mail      */
#define CFG_DEV_FILES       9   /* Upload/download file library          */
#define CFG_DEV_DOORS       10  /* Door PRG files                        */

/* Default drive (partition) within a device */
#define CFG_DRIVE_DEFAULT   0

/* ACIA modem port (6551 SwiftLink at $DE00). Fixed 9600 8N1 via U64. */
#define CFG_ACIA_BASE       0xDE00

/* Maximum number of bulletin boards.
 * (User capacity is USERS_MAX in records.h — a REL-file pre-allocation
 * constraint, not a tunable default.) */
#define CFG_MAX_BOARDS      20

/* Maximum number of polls/votes */
#define CFG_MAX_VOTES       20

/* Maximum number of file areas */
#define CFG_MAX_FILE_AREAS  8

/* Message base limits */
#define CFG_MSG_MAX_PER_BOARD   200   /* max messages per board index */
#define CFG_MSG_LIMIT_DEFAULT   100   /* auto-prune threshold (0=no limit) */
#define CFG_MSG_AGE_DEFAULT       0   /* age-prune threshold days (0=disabled) */
#define CFG_MSG_PRUNE_BATCH      10   /* messages soft-deleted per prune event */
#define CFG_COMPOSE_BUF_FALLBACK 512  /* non-REU compose buffer bytes */
#define CFG_EDITOR_MAX_LINES     30   /* editor hard line limit */
#define CFG_EDITOR_MAX_CHARS   1500   /* editor hard char limit */

/* Maximum number of files in the file library */
#define CFG_MAX_FILES       100

/* Maximum online time per session (minutes) — per access level override
 * in SETUP can reduce this per level */
#define CFG_MAX_SESSION_MIN 60

/* Access level thresholds */
#define CFG_ACCESS_DELETED  0   /* Deleted / banned                      */
#define CFG_ACCESS_NEW      1   /* New unvalidated user                  */
#define CFG_ACCESS_USER     2   /* Standard validated user                */
#define CFG_ACCESS_POWER    3   /* Power user                            */
#define CFG_ACCESS_CO       4   /* Co-SysOp                              */
#define CFG_ACCESS_SYSOP    5   /* SysOp                                 */

/* Terminal output mode (stored per-user; default for new users) */
#define CFG_TERM_DEFAULT    0   /* 0=PETSCII, 1=ASCII, 2=ANSI/CP437     */

/* File channel numbers used for disk I/O (must not conflict) */
#define CFG_FNUM_CMD        15  /* Drive command/status channel          */
#define CFG_FNUM_DATA       8   /* Primary data channel                  */
#define CFG_FNUM_AUX        1   /* Secondary / aux data channel          */
#define CFG_FNUM_MODEM      5   /* RS-232 modem channel                  */

#endif
