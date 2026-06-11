/**
 * TURBO/64 BBS — On-Disk Record Formats
 *
 * These structs define the layout of REL and sequential files on disk.
 * Sizes and field arrangements derived from GST BBS v6.0 BASIC reverse-engineering.
 *
 * REL files are accessed via include/bbs/rel.h; sequential files via include/bbs/hal/disk.h
 */

#ifndef INCLUDE_BBS_RECORDS_H
#define INCLUDE_BBS_RECORDS_H

#include "types.h"
#include "config.h"

/**
 * USER_RECORD — 30 bytes (REL file record)
 *
 * Stored in "usr log,L,30" (relative file).
 * One record per user; record number derived from user ID.
 * Access levels: 0=guest, 1=basic, 2=power, 3=elite, 4=sysop.
 *
 * Terminal settings (term_mode, term_width) persist across calls
 * and enable the BBS to serve diverse callers (Commodore, ASCII, ANSI/CP437).
 */
typedef struct {
  u8  id;                /* User ID (1–255) */
  char handle[16];       /* Handle/username (15 chars, space-padded, null-terminated) */
  char password[4];      /* Password hash (4 chars; CBM charset) */
  u8  access_level;      /* 0–5 */
  u8  credit_balance;    /* Credit points (0–255) */
  u16 calls;             /* Call count */
  u8  downloads;         /* Download count */
  u8  uploads;           /* Upload count */
  u8  term_mode;         /* Terminal mode (0=PETSCII, 1=ANSI_CP437, 2=ASCII) */
  u8  term_width;        /* Column width (40 or 80) */
  u8  term_rows;         /* Row count (24 or 25) */
  u8  flags;             /* Preference flags (byte 29) */
} user_record_t;  /* 30 bytes */

/**
 * USER_PROFILE_RECORD — 86 bytes (REL file record)
 *
 * Stored in "usr prof,L,86" (relative file).
 * One record per user, keyed by ID (matches USR LOG).
 * Loaded only when needed (registration, profile view); never on login scan.
 */
typedef struct {
  u8   id;              /* User ID (matches USR LOG) */
  char email[32];       /* Email address (null-terminated) */
  char firstname[16];   /* First name (null-terminated) */
  char lastname[16];    /* Last name (null-terminated) */
  char location[21];    /* Location or group (null-terminated) */
} user_profile_record_t;

/**
 * BOARD_DIR_RECORD — 44 bytes (REL file record)
 *
 * Stored in "BOARDS,L,44". One slot per board (1-20); empty slots have
 * title all spaces.
 *
 * msg_high_id and body_eof eliminate the two O(n) full-index scans that
 * msg_post() previously required on every message post.  Both counters are
 * zero for a brand-new board and are written back by msg_post(); msg_compact()
 * also updates body_eof after it defragments the TXT file.
 *
 * Migration: records written by an earlier build have these fields as zero.
 * msg_post() detects old-format records (msg_high_id==0 && msg_count>0) and
 * falls back to the scan path on first post, then writes the correct values.
 */
typedef struct {
  u8   id;              /* Board ID 1-20 (0 = empty slot) */
  u8   flags;           /* BOARD_F_* bitmask */
  char title[16];       /* board name, space-padded */
  u16  subop_id;        /* user ID of SubOp (0 = none) */
  u8   read_level;      /* min access level to read (0-5) */
  u8   write_level;     /* min access level to post (0-5) */
  u16  msg_count;       /* cached total non-deleted message count */
  char net_area_tag[8]; /* area tag e.g. "T64.GENL"; zero-filled if local */
  u8   reserved0[4];    /* bytes 32-35: reserved (was board password) */
  u8   max_msgs;        /* per-board prune limit (0 = use CFG_MSG_LIMIT_DEFAULT) */
  u8   max_age_days;    /* age-prune days (0 = disabled) */
  u16  msg_high_id;     /* highest msg_id ever allocated (0 = none yet) */
  u16  body_eof;        /* byte offset of first free byte in B<n>.TXT (0 = empty) */
  u8   display_order;   /* BBS list position 1-255; 0 = legacy, falls back to id */
  u8   reserved[1];     /* padding to 44 bytes */
} board_dir_record_t;  /* 44 bytes */

#define BOARD_F_ANON     0x01
#define BOARD_F_NET      0x02
/* 0x04 free (was BOARD_F_PASSWORD) */
#define BOARD_F_STICKY   0x08

/**
 * MSG_INDEX_RECORD — 63 bytes (REL file record)
 *
 * Stored in "B<n>.IDX,L,63". One record per message (1-based).
 * 200 records = 12.6 KB — REU Bank 0 caches up to 200 records.
 * Bytes 32-62: subj (30 chars + NUL).
 */
typedef struct {
  u16  msg_id;            /* 1-based message number within board */
  u16  parent_id;         /* 0 = thread root; >0 = reply to msg_id */
  u16  thread_root_id;    /* msg_id of thread root (same as msg_id if root) */
  u16  author_id;         /* user ID; 0 = anonymous */
  u16  date;              /* days since 1980-01-01 */
  u16  to_id;             /* recipient user ID; 0 = broadcast (ALL) */
  u8   flags;             /* MSG_F_* bitmask */
  u8   reply_count;       /* direct child count (max 255) */
  u16  body_offset;       /* byte offset into B<n>.TXT */
  u16  body_len;          /* byte length of body in B<n>.TXT */
  char net_origin_bbs[8]; /* originating BBS name; zero-filled if local */
  u16  net_origin_id;     /* msg_id at origin BBS (dedup key on import) */
  u8   month;     /* 1-12, 0 = date not set */
  u8   day;       /* 1-31, 0 = date not set */
  u8   year_yy;  /* 2-digit year (e.g. 26 for 2026), 0 = date not set */
  u8   reserved; /* padding to byte 31 */
  char subj[31];  /* subject (30 chars + NUL); bytes 32-62 */
} msg_index_record_t;  /* 63 bytes */

#define MSG_F_DELETED  0x01
#define MSG_F_FROZEN   0x02
#define MSG_F_ANON     0x04
#define MSG_F_NET      0x08
#define MSG_F_STICKY   0x10
#define MSG_F_ORPHAN   0x20  /* imported reply whose parent not yet received */

/* user_record_t flags (byte 29) */
#define USER_F_CLEAR_ON_MSG  0x01  /* clear screen before each message */

/* access_level_t flags — per-level privilege bitmask (1 byte). */
#define ACCESS_F_POST_ANON     0x01  /* post anonymously */
#define ACCESS_F_PAGE_SYSOP    0x02  /* page sysop for chat */
#define ACCESS_F_SEND_MAIL     0x04  /* send private mail */
#define ACCESS_F_JOIN_POLLS    0x08  /* vote in polls */
#define ACCESS_F_UPLOAD        0x10  /* coarse upload gate */
#define ACCESS_F_NO_TIME_LIMIT 0x20  /* exempt from MINS/DAY */
#define ACCESS_F_NO_CALL_LIMIT 0x40  /* exempt from CALLS/DAY */
#define ACCESS_F_SYSOP         0x80  /* full sysop / co-sysop access */

/**
 * ACCESS_LEVEL — in-memory per-level limits (parsed form of the "access" SEQ file).
 *
 * Persisted in "access" as one comma line per level: "level,name,calls,mins,flags".
 * Not a fixed on-disk record; see src/data/access.c for the file format.
 */
typedef struct {
  u8   level;            /* 0–5 */
  char name[13];         /* editable label, 12 chars + NUL, mixed case, no comma */
  u8   calls_per_day;    /* 0–255 (0 = none allowed) */
  u16  mins_per_day;     /* 0–1440 (0 = none allowed) */
  u8   flags;            /* ACCESS_F_* bitmask */
} access_level_t;

/**
 * USR_PTR_RECORD — 40 bytes (REL file record)
 *
 * Stored in "USR.PTR,L,40". Record number = user ID.
 * hwm[board_id - 1] = last msg_id read on that board (0 = never visited).
 * Pre-allocated for CFG_MAX_USERS users at disk-init.
 */
typedef struct {
  u16  hwm[CFG_MAX_BOARDS];  /* CFG_MAX_BOARDS=20 * 2 bytes = 40 bytes */
} usr_ptr_record_t;  /* 40 bytes */

/**
 * USR_DAY_RECORD — 8 bytes (REL file record)
 *
 * Stored in "USR.DAY,L,8". Record number = user ID. Per-user daily-limit state;
 * counters reset when last_* != the current date (same idea as syscnt.c, but
 * per-user). Created lazily on first write (CBM DOS REL), like USR.PTR.
 */
typedef struct {
  u8  last_mm;        /* month of last call (1-12); 0 = never called */
  u8  last_dd;        /* day of last call (1-31) */
  u8  last_yy;        /* 2-digit year of last call */
  u8  calls_today;    /* calls placed so far today (0-255) */
  u16 mins_today;     /* minutes used so far today (0-1440) */
  u8  reserved[2];    /* pad to 8 bytes */
} usr_day_record_t;   /* 8 bytes */

/**
 * UPLOAD_DOWNLOAD_AREA_RECORD — 40 bytes (REL file record)
 *
 * Stored in "uds,L,40" (upload/download area directory).
 * Defines storage areas; each area has its own file list in "ud<n>,L,100".
 */
typedef struct {
  u8  id;                /* Area ID (1–8) */
  char title[20];        /* Area name (e.g. "Games", "Docs") */
  u8  access_level;      /* Min access level to view/download */
  u8  upload_level;      /* Min access level to upload */
  u8  device;            /* CBM device number (8–11) for this area */
  u8  flags;             /* Reserved / future flags */
  u16 free_blocks;       /* Blocks remaining on device */
  u16 total_files;       /* Number of files in area */
} ud_area_record_t;

/**
 * FILE_ENTRY_RECORD — 100 bytes (REL file record)
 *
 * Stored in "ud<n>,L,100" (per-area file list).
 * One record per file in the area; accessed via area ID.
 */
typedef struct {
  u8  id;                /* File entry ID (1–255) within area */
  char filename[16];     /* CBM filename (space-padded, null-terminated) */
  char description[40];  /* File description (space-padded) */
  u16 blocks;            /* Size in 254-byte blocks */
  char uploader[16];     /* Uploader handle (15 chars + NUL) */
  u16 upload_date;       /* Days since 1980-01-01 */
  u16 downloads;         /* Download count */
  u8  access_level;      /* Min access to download */
  u8  reserved[5];       /* Padding / future use */
} file_entry_record_t;

/**
 * HELP_TEXT_RECORD — 25 bytes (REL file record)
 *
 * Stored in "text,L,25" (help/text page index).
 * Defines offsets into sequential help files (help1–help6).
 */
typedef struct {
  u8  id;                /* Help page ID (1–6) */
  u8  page_num;          /* Sequential file number (1–6) → help<n>.seq */
  u16 seq_offset;        /* Offset in seq file (start of text block) */
  u16 seq_length;        /* Length of text block */
  char title[14];        /* Help page title (space-padded) */
} help_text_record_t;

/**
 * VOTE_RECORD — 40 bytes (REL file record)
 *
 * Stored in "vote1,L,40" (poll/vote questions and answers).
 * Multiple-choice voting system; one record per question.
 */
typedef struct {
  u8  id;                /* Vote ID (1–20) */
  char question[24];     /* Poll question (space-padded) */
  u8  option_count;      /* Number of answer choices (2–6) */
  u8  options[5];        /* Tally count per option (0–5 options) */
  u8  active;            /* 1=active, 0=closed/archived */
} vote_record_t;

/**
 * CONFIG_SETUP — Sequential file (parsed as key=value lines)
 *
 * Stored as "setup" sequential file on device DEV_SYSTEM.
 * Flat text file; parsed line-by-line during BBS boot.
 * Example content:
 *
 *   BBS_NAME=TURBO/64 BBS
 *   BBS_CITY=Retroville
 *   SYSOP_NAME=Admin
 *   SYSOP_PASS=hunter2
 *   MODEM_INIT=ATZ^M
 *   MIN_CALL_TIME=10
 *   MAX_CALL_TIME=60
 *   NEW_USER_LEVEL=1
 *   DEV_SYSTEM=8
 *   DEV_MSGS=9
 *   DEV_FILES=9
 *   DEV_DOORS=10
 *
 * This is NOT a struct; parsed dynamically during cfg_load().
 */

/**
 * CALLERS_LOG — Sequential file (appended-to, not direct-access)
 *
 * Stored as "last" sequential file; each line is a call record.
 * Format: "date time handle access_level call_seconds\n"
 * Example: "05/26 22:45 Elite    4           1234\n"
 * Append-only during logoff; no seek/position needed.
 */

/**
 * MESSAGE BODIES (Bulletins, Mail) — Sequential files
 *
 * Message text stored in separate seq files:
 * - "board<id>.seq" — bulletin board messages (one file per board)
 * - "mail.seq" — private mail bodies
 * Layout: message bodies concatenated; offsets/lengths stored in board/mail REL records.
 * Append-only; offsets tracked in REL index.
 */

/* Record size constants (matching GST BBS on-disk format, extended for terminal settings) */
#define RECORD_SIZE_USER         30  /* id(1)+handle(15)+password(4)+access(1)+credit(1)+calls(2)+dl(1)+ul(1)+mode(1)+width(1)+rows(1)=29, padded to 30 */
#define RECORD_SIZE_USER_PROFILE 86  /* id + email[32] + firstname[16] + lastname[16] + location[21] */
#define RECORD_SIZE_UD_AREA     40
#define RECORD_SIZE_FILE_ENTRY  100
#define RECORD_SIZE_HELP_TEXT   25
#define RECORD_SIZE_VOTE        40
#define RECORD_SIZE_BOARD_DIR   44   /* board_dir_record_t */
#define RECORD_SIZE_MSG_IDX     63   /* msg_index_record_t */
#define RECORD_SIZE_USR_PTR     40   /* usr_ptr_record_t */
#define RECORD_SIZE_USR_DAY      8    /* usr_day_record_t */

/* Maximum users in the user database.
 * Must be specified at REL file creation time (all records are pre-allocated
 * on disk so the scan in user_by_handle can read sequentially without P
 * commands, which is the reliable CBM DOS approach used by c*base et al.) */
#define USERS_MAX               100

#endif /* INCLUDE_BBS_RECORDS_H */
