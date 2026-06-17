/* DIAGNOSTIC spike host — screen-poke markers only (no scrolling prints).
 * Screen RAM $0400, color RAM $D800. Markers freeze on screen so we can see
 * exactly how far execution gets even if the machine then loops/hangs.
 *   line 0, col 0-3: '1'=host start  '2'=door loaded  '3'=returned  O/X=ZP ok/corrupt
 *   line 1, col 0-3: 'A'=door entered 'B'=DCB read   'C'=magic ok  'D'=callback returned
 *   line 3, col 0+:  rendered callback string (host_print output)
 *
 * Round 3 — BBS build mode (-Os -Oo, no -n).  Three additions vs Round 2:
 *
 * 1. host_print is #pragma native: the door calls it via a function pointer;
 *    as a native wrapper it calls host_emit (no #pragma native).
 *    This exercises the real BBS path: native door → native wrapper → non-native body.
 *
 * 2. host_emit has no #pragma native: it represents the non-explicitly-native BBS
 *    function body.  With -Os -Oo oscar64 compiles it native anyway (all code native),
 *    but the structural distinction (no pragma) matches how real BBS wrapper→impl works.
 *
 * 3. call_door is #pragma native + uses hand __asm X-indexed copy for ZP save/restore.
 *    Self-corruption risk: a C loop compiled as bytecode runs ON the ZP it restores,
 *    so the bytecode runtime's own ZP state (SP/IP/ACCU/Tn) would be clobbered mid-loop.
 *    The hand-asm X-indexed copy uses only A and X (no ZP scratch), so it is safe.
 *
 * ZP save/restore: oscar64 uses ZP $00-$F6 for runtime state (IP, SP, ACCU, Tn, Pn).
 * We save $02-$8F around the door JSR to protect the host's live ZP state.
 * $00-$01 are 6502 reserved; $02 is our sentinel. Upper bound $8F covers all named
 * ZP vars observed in BOOT-0.1.0.asm (highest: T12 at $64; $8F leaves headroom). */
#include <c64/kernalio.h>

#define SCR ((volatile unsigned char *)0x0400)
#define COL ((volatile unsigned char *)0xD800)
#define DCB ((volatile unsigned char *)0x033C)

/* ZP range to save/restore around the door call */
#define ZP_SAVE_LO  0x02
#define ZP_SAVE_HI  0x8F
#define ZP_SAVE_LEN (ZP_SAVE_HI - ZP_SAVE_LO + 1)   /* = 142 bytes */

static unsigned char zp_buf[ZP_SAVE_LEN];

/* NON-NATIVE rendering body — proves native wrapper → normal-compiled body chain.
 * No #pragma native; with -Os -Oo oscar64 still generates native machine code for it
 * (the compiler's optimizer always converts to native), but the function is NOT
 * explicitly native — it would be bytecode in a non-optimized (-O0) build.
 * __noinline keeps it a separate function so the JSR from host_print is visible. */
__noinline static void host_emit(const char *s) {
  unsigned p = 120;
  while (*s) {
    unsigned char c = *s++;
    if (c == '\r') break;
    if (c >= 0x41 && c <= 0x5A) c -= 0x40;   /* ASCII uppercase -> screen code */
    SCR[p] = c; COL[p] = 1; p++;
  }
}

/* NATIVE wrapper: door calls this via function pointer; it then calls host_emit.
 * Must be #pragma native so the door's indirect dispatch (JMP ACCU) enters native code
 * directly without needing the HOST bcexec trampoline, and so the compiler emits it as
 * a proper native function with a standard entry point callable from outside the host. */
static void host_print(const char *s) {
  host_emit(s);
}
#pragma native(host_print)

struct api { unsigned char version; void (*print)(const char *); };
static struct api g_api;

static void mark(unsigned char pos, unsigned char code) { SCR[pos] = code; COL[pos] = 1; }

/* NATIVE: contains JSR $9700 and ZP save/restore.
 * Must be native because:
 *   a) inline __asm requires a native function context in oscar64, and
 *   b) the restore must run without ANY ZP temporaries (self-corruption risk).
 * Hand __asm X-indexed copy: LDA $02,x / STA buf,x / INX — uses only A and X,
 * no ZP scratch within $02-$8F. */
static void call_door(void) {
  /* Save $02-$8F into zp_buf using X-indexed absolute addressing (no ZP scratch) */
  __asm {
    ldx #0
  save_loop:
    lda $02,x
    sta zp_buf,x
    inx
    cpx #ZP_SAVE_LEN
    bne save_loop
  }

  __asm { jsr $9700 }

  /* Restore $02-$8F from zp_buf — same X-indexed pattern, no ZP scratch */
  __asm {
    ldx #0
  restore_loop:
    lda zp_buf,x
    sta $02,x
    inx
    cpx #ZP_SAVE_LEN
    bne restore_loop
  }
}
#pragma native(call_door)

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
