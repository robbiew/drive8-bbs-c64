/* devkit/door_crt.c — door startup. Author supplies void door_main(void). */
#include "bbs/door_abi.h"

/* The door image occupies $9700-$C000.  Oscar64 emits it as DOOR.prg (load
 * address $9700) alongside a discardable stub PRG.  Build with -n -O2. */
#pragma overlay( DOOR, 1 )
#pragma section( door_hdr_code, 0 )   /* 3-byte JMP at $9700           */
#pragma section( door_hdr_data, 0 )   /* 3 magic/ABI bytes at $9703    */
#pragma section( door_code,     0 )   /* bcexec + door_entry at $9706+ */
#pragma section( door_bss,      0, , , bss )
#pragma region( door, 0x9700, 0xC000, , 1, \
                {door_hdr_code, door_hdr_data, door_code, door_bss} )

/* ── $9700: 3-byte JMP ────────────────────────────────────────────────────
 * The BBS does `jsr $9700`; the JMP forwards execution to door_entry.
 * door_run validates $9703/$9705 BEFORE entering, so the JMP is never
 * taken on an ABI-mismatched image.
 * Kept alive: _jmp_keep (BSS in stub) is assigned door_jmp_hdr at runtime
 * in main() and then called — the compiler cannot eliminate it. */
#pragma code(door_hdr_code)
void door_entry(void);
__asm door_jmp_hdr { jmp door_entry }

/* ── $9703: magic + ABI bytes ─────────────────────────────────────────────
 * door_run checks hdr[BBS_DOOR_HDR_MAGIC]='D', hdr[BBS_DOOR_HDR_MAGIC+1]='6',
 * hdr[BBS_DOOR_HDR_VER]=BBS_ABI_VERSION before calling enter_door().
 * volatile: forces emission in-region and prevents constant folding.
 * Kept alive: main() returns _door_magic[0] — the read is live. */
#pragma data(door_hdr_data)
static volatile u8 _door_magic[3] = {
    BBS_DOOR_MAGIC0,   /* 0x44 = 'D' at $9703 */
    BBS_DOOR_MAGIC1,   /* 0x36 = '6' at $9704 */
    BBS_ABI_VERSION    /* 0x01       at $9705 */
};

/* ── $9706+: in-region bcexec + door_entry ────────────────────────────────*/
#pragma code(door_code)

/* In-region indirect-call trampoline: door makes NO sub-$9700 calls.
 * In native mode (-n), bcexec is just JMP (accu) = JMP ($001B). */
__asm door_bcexec { jmp (accu) }
#pragma runtime(bcexec, door_bcexec)

void door_main(void);            /* supplied by the door author */

#pragma bss(door_bss)
static u8 host_sp_lo, host_sp_hi;   /* SP save scratch — must live in-region */
#pragma bss(bss)

/* door_entry: save host Oscar64 SP ($23/$24), install door-local SP at the
 * top of the door region ($BFFE), run door_main, restore host SP.
 * JSR-safe: no TXS, does not touch the hardware stack.  The BBS enters via
 * `jsr $9700` → JMP door_entry, then saves/restores ZP $02-$8F around it. */
void door_entry(void) {
    host_sp_lo = *((volatile u8 *)0x23);
    host_sp_hi = *((volatile u8 *)0x24);
    *((volatile u8 *)0x23) = 0xFE;
    *((volatile u8 *)0x24) = 0xBF;
    door_main();
    *((volatile u8 *)0x23) = host_sp_lo;
    *((volatile u8 *)0x24) = host_sp_hi;
}
#pragma native(door_entry)

/* ── Stub (discarded PRG): keeps door_hdr_code + door_hdr_data alive ─────
 * Oscar64 emits two PRGs: DOOR.prg (the overlay at $9700, used by the BBS)
 * and the stub at $0801 (discarded).  The stub's main() forces the compiler
 * to include door_jmp_hdr and _door_magic in the overlay:
 *   - _jmp_keep (BSS) is assigned door_jmp_hdr at runtime then called, so
 *     the compiler cannot prove door_jmp_hdr is dead and must emit it.
 *   - main() returns _door_magic[0] (volatile read) — the return value is
 *     live, preventing the volatile array from being DCE'd. */
#pragma code(code)

static void (* volatile _jmp_keep)(void);   /* BSS: NOT placed in door region */
void (*door_entry_ptr)(void) = door_entry;

int main(void) {
    _jmp_keep = door_jmp_hdr;
    _jmp_keep();
    door_entry_ptr();
    return (int)_door_magic[0];
}
