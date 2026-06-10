/* C64 CIA1 TOD clock implementation. */
#include "bbs/hal/clock.h"

/* CIA1 TOD registers */
#define CIA1_TOD_10THS  (*(volatile u8 *)0xDC08)
#define CIA1_TOD_SEC    (*(volatile u8 *)0xDC09)
#define CIA1_TOD_MIN    (*(volatile u8 *)0xDC0A)
#define CIA1_TOD_HOURS  (*(volatile u8 *)0xDC0B)
#define CIA1_CRA        (*(volatile u8 *)0xDC0E)

/* BCD to binary */
static u8 bcd_to_bin(u8 bcd)
{
    return (u8)((bcd >> 4) * 10 + (bcd & 0x0F));
}

/* Binary to BCD */
static u8 bin_to_bcd(u8 bin)
{
    return (u8)(((bin / 10) << 4) | (bin % 10));
}

void clock_init(void)
{
    /* Set TOD clock to 50 Hz (PAL). CRA bit 7 = 1 for 50 Hz, 0 for 60 Hz.
     * Most C64/U64 users are in PAL territory; adjust if needed. */
    CIA1_CRA = (u8)(CIA1_CRA | 0x80);

    /* Set clock to 00:00:00.0 — write hours first (stops TOD),
     * write tenths last (starts TOD running). */
    CIA1_TOD_HOURS = 0x01;   /* TOD hours 0 is invalid; start at 1:00 AM */
    CIA1_TOD_MIN   = 0x00;
    CIA1_TOD_SEC   = 0x00;
    CIA1_TOD_10THS = 0x00;
}

void clock_read(clock_tod_t *t)
{
    /* Reading $DC0B latches all four registers until $DC08 is read. */
    t->hours  = CIA1_TOD_HOURS;
    t->mins   = CIA1_TOD_MIN;
    t->secs   = CIA1_TOD_SEC;
    t->tenths = CIA1_TOD_10THS;   /* reading this unlatches */
}

void clock_set(const clock_tod_t *t)
{
    /* Write $DC0B (hours) first — this stops the TOD and enters "set" mode.
     * Write $DC08 (tenths) last — this starts the TOD running again. */
    CIA1_TOD_HOURS = t->hours;
    CIA1_TOD_MIN   = t->mins;
    CIA1_TOD_SEC   = t->secs;
    CIA1_TOD_10THS = t->tenths;
}

u32 clock_to_secs(const clock_tod_t *t)
{
    u8 h = bcd_to_bin(t->hours & 0x7F);   /* strip PM bit */
    u8 m = bcd_to_bin(t->mins);
    u8 s = bcd_to_bin(t->secs);
    /* Handle 12-hour clock: 12 AM = 0, 12 PM = 12 */
    if (h == 12) h = (t->hours & 0x80) ? 12 : 0;
    else if (t->hours & 0x80) h += 12;
    return (u32)h * 3600 + (u32)m * 60 + s;
}

u16 clock_elapsed(const clock_tod_t *start, const clock_tod_t *end)
{
    u32 s = clock_to_secs(start);
    u32 e = clock_to_secs(end);
    if (e < s) e += 86400;   /* midnight wrap */
    u32 diff = e - s;
    return (u16)(diff > 65535 ? 65535 : diff);
}

void clock_fmt(const clock_tod_t *t, char *buf)
{
    u8 h = bcd_to_bin(t->hours & 0x7F);
    u8 m = bcd_to_bin(t->mins);
    u8 s = bcd_to_bin(t->secs);
    buf[0] = (char)('0' + h / 10);
    buf[1] = (char)('0' + h % 10);
    buf[2] = ':';
    buf[3] = (char)('0' + m / 10);
    buf[4] = (char)('0' + m % 10);
    buf[5] = ':';
    buf[6] = (char)('0' + s / 10);
    buf[7] = (char)('0' + s % 10);
    buf[8] = '\0';
}
