/* DIAGNOSTIC spike host — screen-poke markers only (no scrolling prints).
 * Screen RAM $0400, color RAM $D800. Markers freeze on screen so we can see
 * exactly how far execution gets even if the machine then loops/hangs.
 *   line 0, col 0-3: '1'=host start  '2'=door loaded  '3'=returned  O/X=ZP ok/corrupt
 *   line 1, col 0-3: 'A'=door entered 'B'=DCB read   'C'=magic ok  'D'=callback returned
 *   line 3, col 0+:  rendered callback string (host_print output)
 *
 * ZP save/restore: oscar64 uses ZP $00-$F6 for runtime state (IP, SP, ACCU, Tn, Pn).
 * We save $02-$8F around the door JSR to protect the host's live ZP state.
 * $00-$01 are 6502 reserved; $02 is our sentinel. Upper bound $8F is generous
 * (highest named ZP var observed in HOST.asm is $54; $8F leaves headroom). */
#include <c64/kernalio.h>

#define SCR ((volatile unsigned char *)0x0400)
#define COL ((volatile unsigned char *)0xD800)
#define DCB ((volatile unsigned char *)0x033C)

/* ZP range to save/restore around the door call */
#define ZP_SAVE_LO  0x02
#define ZP_SAVE_HI  0x8F
#define ZP_SAVE_LEN (ZP_SAVE_HI - ZP_SAVE_LO + 1)   /* = 142 bytes */

static unsigned char zp_buf[ZP_SAVE_LEN];

/* api target: render at a FIXED screen spot (row 3, offset 120) so the door's
 * callback output is visible without scrolling away the trace markers. */
static void host_print(const char *s) {
  unsigned p = 120;
  while (*s) {
    unsigned char c = *s++;
    if (c == '\r') break;
    if (c >= 0x41 && c <= 0x5A) c -= 0x40;   /* ASCII uppercase -> screen code */
    SCR[p] = c; COL[p] = 1; p++;
  }
}

struct api { unsigned char version; void (*print)(const char *); };
static struct api g_api;

static void mark(unsigned char pos, unsigned char code) { SCR[pos] = code; COL[pos] = 1; }

static void call_door(void) {
  unsigned char i;
  /* Save host ZP state before door entry (door's oscar64 runtime will clobber it) */
  for (i = 0; i < ZP_SAVE_LEN; i++)
    zp_buf[i] = *((volatile unsigned char *)(ZP_SAVE_LO + i));

  __asm { jsr $9700 }

  /* Restore host ZP state so host oscar64 runtime continues cleanly */
  for (i = 0; i < ZP_SAVE_LEN; i++)
    *((volatile unsigned char *)(ZP_SAVE_LO + i)) = zp_buf[i];
}

int main(void) {
  unsigned i;
  for (i = 0; i < 1000; i++) { SCR[i] = 0x20; COL[i] = 1; }  /* clear screen */
  *((volatile unsigned char *)0x02) = 0x5A;   /* ZP sentinel */
  g_api.version = 1;
  g_api.print   = host_print;
  DCB[0] = 'D'; DCB[1] = '6'; DCB[2] = 1;
  DCB[3] = (unsigned)&g_api & 0xFF;
  DCB[4] = ((unsigned)&g_api >> 8);

  mark(0, 0x31);                              /* '1' host reached start */
  krnio_setnam("DOOR");
  if (!krnio_load(1, 8, 1)) { mark(0, 0x09); for(;;); }  /* 'I' = load failed */
  mark(1, 0x32);                              /* '2' door loaded, about to JSR */
  call_door();                                /* enter door at $9700 */
  mark(2, 0x33);                              /* '3' returned from door */
  if (*((volatile unsigned char *)0x02) == 0x5A) mark(3, 0x0F); /* 'O' ZP ok */
  else mark(3, 0x18);                                            /* 'X' ZP corrupt */
  for(;;);                                    /* freeze screen */
}
