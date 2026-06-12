/**
 * TURBO/64 BBS — Session State Machine
 *
 * Models a single caller's BBS session from dial-in to logoff.
 * Single-line operation: only one session at a time.
 */

#ifndef INCLUDE_BBS_SESSION_H
#define INCLUDE_BBS_SESSION_H

#include "types.h"
#include "err.h"
#include "records.h"
#include "term.h"
#include "bbs/spy80.h"

/**
 * Menu navigation state structure
 * (Defined here to avoid circular header dependency with menu.h)
 */
typedef struct menu_state_s {
  char    current_menu[16];    /* ID of currently displayed menu */
  u8      depth;               /* Nesting depth (0 = MAIN menu) */
  char    parent_menus[5][16]; /* Stack of parent menu IDs (5-level max) */
  u8      unknown_count;       /* Unknown commands this session; cap forces logoff */
} menu_state_t;

/**
 * Spy terminal mode selection
 */
typedef enum {
  SPY_MODE_PETSCII = 0,  /* 40-col character RAM path (default) */
  SPY_MODE_80COL   = 1   /* hires bitmap path */
} spy_mode_t;

/**
 * Spy terminal state for WFC user monitoring.
 * The PETSCII virtual terminal writes directly to screen RAM, bypassing KERNAL.
 * Cursor position and color are tracked as module statics in session.c.
 */
typedef struct screen_capture_s {
  bool_t     wfc_show_user_view;   /* TRUE = show spy view, FALSE = show WFC menu */
  spy_mode_t spy_mode;             /* SPY_MODE_PETSCII or SPY_MODE_80COL */
} screen_capture_t;

/**
 * Session state enumeration
 */
typedef enum {
  SESS_IDLE,           /* Waiting for carrier detect */
  SESS_CONNECTED,      /* Carrier detected, waiting for login */
  SESS_AUTHENTICATING, /* Processing login/registration */
  SESS_REGISTERING,    /* New user registration flow */
  SESS_AUTHENTICATED,  /* User logged in, at main menu */
  SESS_IN_MENU,        /* Processing menu command */
  SESS_LOGOFF,         /* User requested logoff */
  SESS_TIMEOUT,        /* Call time limit exceeded */
  SESS_DISCONNECTING,  /* Dropping carrier */
  SESS_ERROR           /* Unrecoverable session error */
} session_state_t;

/**
 * Main session structure
 */
typedef struct session_s {
  session_state_t state;
  bbs_err_t       error;      /* Last error code (if state == SESS_ERROR) */

  /* User data */
  user_record_t   user;       /* Current logged-in user (all-zero if not auth'd) */
  u8              user_id;    /* User ID (0 if anonymous) */
  char            handle[16]; /* Handle buffer (null-terminated) */

  /* Call timing */
  u16 call_start_secs;  /* TOD seconds when call started */
  u16 call_elapsed;     /* Seconds online so far */
  u8  online_limit;     /* Max seconds for this user (from cfg/access level) */

  /* Terminal mode negotiation */
  term_mode_t     term_mode;      /* PETSCII, ANSI, ASCII */
  u8              term_width;     /* Column width: 40 or 80 */
  u8              term_rows;      /* Row count: 24 or 25 */
  bool_t          ansi_color;     /* Terminal supports ANSI color codes */
  bool_t          ansi_graphics;  /* Terminal supports IBM-style graphics */
  bool_t          linefeed_mode;  /* Terminal needs LF after CR */
  bool_t          petscii_lower;  /* PETSCII caller is in lowercase/text charset */

  /* Menu/command state */
  char            last_cmd;   /* Last menu command char */
  char            input_buf[64]; /* General-purpose input buffer */
  bool_t          menu_displayed;   /* TRUE if menu shown for this session (Phase B) */
  bool_t          menu_needs_pause; /* TRUE after action output — pause before redraw */
  bool_t          menu_skip_pause;  /* handler sets TRUE to suppress the post-action pause */

  /* Auth state tracking */
  u8              auth_step;  /* 0=awaiting handle, 1=awaiting password */
  u8              login_attempts; /* Failed login count; drop carrier at 3 */
  u8              empty_handle_attempts; /* Blank handle count; drop carrier at 3 */
  char            password[12]; /* Password input buffer */

  /* New user registration state */
  u8              reg_step;           /* Sub-step within SESS_REGISTERING (0-12) */
  u8              reg_editing;        /* Non-zero when re-editing a field from confirmation */
  char            reg_confirm[12];    /* Password confirmation buffer */
  char            reg_email[32];      /* Email address */
  char            reg_firstname[16];  /* First name */
  char            reg_lastname[16];   /* Last name */
  char            reg_location[21];   /* Location or group */

  /* Flags */
  bool_t is_local;      /* TRUE if no modem (sysop local mode) */
  bool_t is_new_user;   /* TRUE if registering new account */
  bool_t sysop_page;    /* TRUE if sysop page in progress */

  /* Menu navigation state (Phase B — menu/prompt system) */
  menu_state_t menu_state;    /* Current menu context and history */

  /* Screen capture for WFC user monitoring */
  screen_capture_t screen_capture;  /* Captures output during session for sysop view */
} session_t;

/**
 * session_init()
 *
 * Initialize a new session. Called when carrier detect fires.
 *
 * Returns:
 *   BBS_OK — session ready
 */
bbs_err_t session_init(session_t *s, bool_t is_local);

/**
 * session_step()
 *
 * Single step of the session state machine.
 * Processes current state, reads input from modem/keyboard, updates state.
 *
 * Must be called in a loop until session returns to SESS_IDLE.
 *
 * Returns:
 *   BBS_OK         — normal progression
 *   BBS_EPROTO     — protocol error (bail)
 *   BBS_EFATAL     — unrecoverable (halt BBS)
 */
bbs_err_t session_step(session_t *s);

/**
 * session_emit()
 *
 * Emit text to the current session (to modem + optional local screen).
 * Handles translation per term_mode (PETSCII, ANSI, ASCII).
 */
void session_emit(const session_t *s, const char *text);

/**
 * session_clear_screen()
 *
 * Clear the terminal screen in a mode-appropriate way:
 *   PETSCII   — CHR$(147) (CLR/HOME)
 *   ANSI      — ESC[2J ESC[H
 *   ASCII     — 24 blank lines
 */
void session_clear_screen(const session_t *s);

/**
 * session_display_file()
 *
 * Phase B: Display a terminal-aware file from disk.
 * Uses disk_open_with_fallback() to find the file, then reads and
 * displays it line-by-line via session_emit().
 *
 * Parameters:
 *   s         - session with terminal settings and I/O channels
 *   prefix    - file type prefix character ('g'=gfile, 'm'=menu, 'p'=prompt)
 *   base_name - base filename (e.g., "menu", "login", "detect")
 *
 * Inline pipe color codes like |03 are recognized in display files and
 * translated to the active terminal's color controls.
 *
 * Returns:
 *   BBS_OK        - file displayed successfully
 *   BBS_ENOTFOUND - no file candidate found
 *   BBS_EIO       - error reading or displaying file
 *
 * Note: The file lookup uses s->term_mode and s->term_width to find
 * the best match via the fallback chain:
 *   1. s.<name> <mode> <width>  (where s = prefix)
 *   2. s.<name> <mode>
 *   3. s.<name> <width>
 *   4. s.<name>
 */
bbs_err_t session_display_file(const session_t *s, char prefix, const char *base_name);

/**
 * session_set_mci_board()
 * Set the board-name MCI context used by session_display_file() to substitute
 * %BN tokens in gfiles.  title16 is the 16-byte space-padded title field from
 * board_dir_record_t; it is trimmed and stored internally.  Pass NULL to clear.
 */
void session_set_mci_board(const char *title16);

/**
 * session_set_mci_msg()
 * Set the message-number MCI context for %MN substitution in display files.
 * Typically called with s_cur_msg before displaying p.read.
 */
void session_set_mci_msg(u16 msg_num);

/* Emit a single raw control byte to the caller (e.g. 0x0E text charset,
 * 0x8E graphics charset), bypassing CP437 translation. */
void session_emit_charset(const session_t *s, u8 code);

/* TRUE when the local C64 screen is currently in lowercase/text charset. Used by
 * the WFC footer to render its labels with the correct case. */
bool_t session_screen_lower(void);

/* Low-level I/O primitives used by session sub-modules */
void sess_tx(const char *text);
/* Read one byte from the caller (keyboard in local mode, modem otherwise).
 * Returns TRUE if a byte was read, FALSE if no carrier / timeout. */
bool_t sess_getc(u8 *out);
/* TRUE while the session can still talk to the caller: local sessions are
 * always live; remote sessions require modem carrier. */
bool_t sess_carrier_ok(const session_t *s);
void sess_erase_char(const session_t *s);
/* Blocking CR-terminated line reader with echo, BS/DEL erase, and optional
 * a-z->A-Z folding. Stops early (keeping what was collected) if carrier drops. */
void sess_read_line(const session_t *s, char *buf, u8 max, bool_t uppercase);
/* Emit a terminal color/attribute byte (PETSCII) or ANSI escape.
 * No-op for ASCII mode. */
void sess_color(const session_t *s, u8 petscii_code, const char *ansi_code);
/* Reset to default colors: white foreground, black background, no reverse. */
void sess_reset_color(const session_t *s);
/* Emit color for pipe code index 0-17 (same mapping as |NN in gfiles). */
void sess_pipe_color(const session_t *s, u8 color);

/**
 * PETSCII spy terminal helpers for WFC user monitoring.
 */
void session_spy_init(session_t *s);          /* Init spy terminal and clear rows 0–19 */
void session_spy_feed(u8 ch);                 /* Feed one PETSCII byte to spy terminal */
/* Draw the 80-col spy SysOp status line (row 24): identity + action-key hints. */
void session_spy_status(const session_t *s, u16 elapsed_secs);
/* Resident SysOp tick: poll action keys + refresh the status line (once/sec).
 * Call from the main session loop and from feature loops (message reader, etc.)
 * so the spy keeps ticking and keys stay live wherever the caller navigates. */
void session_spy_poll(void);

/**
 * session_done()
 *
 * Clean up session (write logoff record, reset modem, etc).
 * Called when session reaches SESS_IDLE or error state.
 */
bbs_err_t session_done(session_t *s);

#endif /* INCLUDE_BBS_SESSION_H */
