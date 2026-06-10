/**
 * bbs/sysop.h — WFC (Waiting For Caller) sysop screen
 *
 * C*BASE-style layout:
 *   Row  0: time | BBS name          (updates each second)
 *   Row  1: separator
 *   Rows 2–7: two-column statistics grid
 *   Row  8: separator
 *   Row  9: status banner
 *   Row 10: separator
 *   Rows 11–17: caller activity log (7 most recent)
 *   Row 18: separator
 *   Rows 19–23: command menu (two columns)
 *   Row 24: alert line (no trailing newline)
 */
#ifndef INCLUDE_BBS_SYSOP_H
#define INCLUDE_BBS_SYSOP_H

#include "types.h"
#include "err.h"
#include "session.h"
#include "hal/clock.h"

/* Number of activity log rows shown on screen */
#define WFC_LOG_SIZE  0

/**
 * Activity log entry — one completed caller session
 */
typedef struct {
    clock_tod_t time;        /* TOD at session start */
    char        handle[16];  /* user handle (15 chars + NUL) */
    u16         baud;        /* connect baud rate */
    u16         duration;    /* session length in seconds */
} wfc_log_entry_t;

/**
 * WFC runtime state (global, zero-init at boot)
 */
typedef struct {
    clock_tod_t     boot_time;          /* TOD recorded at wfc_init() */
    wfc_log_entry_t log[WFC_LOG_SIZE];  /* circular activity log */
#if WFC_LOG_SIZE > 0
    u8              log_count;          /* entries populated (0–WFC_LOG_SIZE) */
    u8              log_head;           /* next write index (circular) */
#endif
    u8              last_secs;          /* last-drawn seconds (detect change) */
    char            date[9];            /* current date "MM/DD/YY" + NUL */
    bool_t          local_logon;        /* CR/F4: sysop local-session requested */
    bool_t          guest_enabled;      /* F3: guest access toggle */
    u16             calls_today;        /* calls logged since last boot */
    u16             posts_today;        /* messages posted since last boot */
    char            chat_msg[21];       /* sysop status message (20 chars + NUL); set via F6 */
    bool_t          ovl_wfc_loaded;     /* overlay bank 2 (WFC code) loaded */
    bool_t          chat_enabled;       /* sysop chat availability — shown as SYSOP CHAT Y/N */
} wfc_state_t;

/** Global WFC state — accessible to main.c */
extern wfc_state_t wfc;

/* WFC overlay impl functions — defined in wfc_code section, linked to $9D80+ */
void       wfc_init_impl(void);
void       wfc_display_impl(void);
bbs_err_t  wfc_update_impl(void);

/**
 * wfc_set_datetime()
 * Prompt sysop to enter current time and date.
 * Sets CIA1 TOD clock and stores date string in wfc.date.
 * Call at boot and whenever F5 is pressed on WFC screen.
 */
void wfc_set_datetime(void);

/**
 * wfc_init()
 * Initialize WFC state and start the CIA1 TOD clock.
 * Call once at boot after cfg_init().
 */
void wfc_init(void);

/**
 * wfc_display()
 * Clear screen and draw full C*BASE-style WFC layout.
 * Call once at boot and after each session ends.
 */
void wfc_display(void);

/**
 * wfc_update()
 * Poll modem and keyboard; update time field in place.
 * Call in tight loop while idle (waiting for caller).
 *
 * Returns:
 *   BBS_OK    — still idle, keep waiting
 *   BBS_EAGAIN — carrier detected; caller is calling
 */
bbs_err_t wfc_update(void);

/**
 * wfc_reload()
 * Reload the WFC overlay (bank 2) if it was displaced by OVL_MSGS.
 * Call after returning from a feature that loads another overlay (e.g. the
 * message bases) so the in-session spy view code becomes resident again.
 */
void wfc_reload(void);

/**
 * wfc_log_session()
 * Append a completed session to the activity log buffer.
 * Call after session_done() before calling wfc_display().
 */
void wfc_log_session(const char *handle, u16 baud, u16 duration);

/**
 * wfc_display_session()
 * Display WFC screen during active session.
 * Initialises PETSCII spy terminal, clears rows 0-19, enters spy view.
 * Call once before entering the session loop.
 */
void wfc_display_session(const session_t *s);

/**
 * wfc_update_session()
 * Update WFC display during active session.
 * Handles F1 toggle between user view and WFC view.
 * User view: displays captured screen + user info footer, updates footer each second.
 * WFC view: shows normal WFC menu (stats, banner, log).
 * Call repeatedly in session loop.
 */
void wfc_update_session(const session_t *s, u16 elapsed_secs);


#endif /* INCLUDE_BBS_SYSOP_H */
