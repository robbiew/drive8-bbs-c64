/* bbs/hal/clock.h - C64 CIA1 TOD (time-of-day) clock abstraction.
 *
 * The C64 CIA1 chip has a hardware BCD time-of-day clock at:
 *   $DC08 = 10ths of seconds
 *   $DC09 = seconds  (BCD)
 *   $DC0A = minutes  (BCD)
 *   $DC0B = hours    (BCD, bit 7 = PM flag)
 *
 * The BBS uses the TOD clock to:
 *   - Timestamp caller log entries
 *   - Enforce per-session time limits
 *   - Gate the SysOp status display refresh
 */
#ifndef BBS_HAL_CLOCK_H
#define BBS_HAL_CLOCK_H

#include "bbs/types.h"

typedef struct {
    u8 tenths;   /* 0x00-0x09 (BCD) */
    u8 secs;     /* 0x00-0x59 (BCD) */
    u8 mins;     /* 0x00-0x59 (BCD) */
    u8 hours;    /* 0x01-0x12 (BCD); bit 7 set = PM */
} clock_tod_t;

/* Start the TOD clock oscillator and set it to 1:00:00.0.
 * tod_hz declares what frequency the CIA's TOD pin is fed at — 60, or 50 for
 * any other value.  Getting it wrong makes the clock run 20% fast or slow, and
 * it cannot be inferred from the video standard, so the boot path measures it
 * (wfc_init_impl) rather than assuming. Must be called once at boot. */
void clock_init(u8 tod_hz);

/* Read the current time into *t.  Reads all four registers atomically
 * (CIA latches them when $DC0B is read). */
void clock_read(clock_tod_t *t);

/* Set the current time from *t. */
void clock_set(const clock_tod_t *t);

/* Convert a clock_tod_t to seconds since midnight (0-86399). */
u32 clock_to_secs(const clock_tod_t *t);

/* Return elapsed seconds between two readings (handles midnight wrap). */
u16 clock_elapsed(const clock_tod_t *start, const clock_tod_t *end);

/* Format a clock_tod_t as "HH:MM:SS" into buf (must be >= 9 bytes). */
void clock_fmt(const clock_tod_t *t, char *buf);

#endif /* BBS_HAL_CLOCK_H */
