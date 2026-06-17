/* devkit/door_crt.h — T/64 door startup (include this; then define door_main).
 *
 * A door is built as ONE translation unit: your .c does
 *     #include "door_crt.h"
 *     void door_main(void) { ... }
 * and `make door DOOR=<name>` compiles just that file.  This header ends by
 * making door_code/door_data/door_bss the active sections, so EVERYTHING you
 * write after the include — code, string literals, statics — lands inside the
 * $9700-$C000 image that the BBS loads.  (Splitting startup and door into two
 * files put the door's strings in the main region, which isn't loaded — they
 * printed as garbage.)
 *
 * Build flags: -n -O2.  Doors must make NO calls below $9700, so avoid /, %,
 * *, float and long math (they pull in Oscar64 runtime helpers at $09xx). */
#ifndef T64_DOOR_CRT_H
#define T64_DOOR_CRT_H

#include "door_sdk.h"   /* bbs_api_t, bbs_caller_t, bbs() */

#pragma overlay( DOOR, 1 )
#pragma section( door_hdr_code, 0 )   /* 3-byte JMP at $9700              */
#pragma section( door_hdr_data, 0 )   /* magic/ABI bytes at $9703         */
#pragma section( door_code,     0 )   /* bcexec + door_entry + door_main  */
#pragma section( door_data,     0 )   /* string literals + const data     */
#pragma section( door_bss,      0, , , bss )
#pragma region( door, 0x9700, 0xC000, , 1, \
                {door_hdr_code, door_hdr_data, door_code, door_data, door_bss} )

void door_main(void);   /* supplied by the door author, after this include */

/* $9700: JMP door_entry — the BBS does `jsr $9700`. */
#pragma code(door_hdr_code)
void door_entry(void);
__asm door_jmp_hdr { jmp door_entry }

/* $9703: magic + ABI bytes — door_run validates these before entering. */
#pragma data(door_hdr_data)
static volatile u8 _door_magic[3] = {
    BBS_DOOR_MAGIC0, BBS_DOOR_MAGIC1, BBS_ABI_VERSION
};

/* In-region indirect-call trampoline (native bcexec = JMP (accu)). */
#pragma code(door_code)
__asm door_bcexec { jmp (accu) }
#pragma runtime(bcexec, door_bcexec)

#pragma bss(door_bss)
static u8 _door_sp_lo, _door_sp_hi;
#pragma bss(bss)

/* door_entry: save host Oscar64 SP, install a door-local SP at $BFFE, run the
 * author's door_main, restore host SP.  JSR-safe (no TXS). */
void door_entry(void) {
    _door_sp_lo = *((volatile u8 *)0x23);
    _door_sp_hi = *((volatile u8 *)0x24);
    *((volatile u8 *)0x23) = 0xFE;
    *((volatile u8 *)0x24) = 0xBF;
    door_main();
    *((volatile u8 *)0x23) = _door_sp_lo;
    *((volatile u8 *)0x24) = _door_sp_hi;
}
#pragma native(door_entry)

/* Discardable stub main() ($0801): anchors door_jmp_hdr + _door_magic so the
 * compiler emits them into the overlay.  The BBS never runs this. */
#pragma code(code)
static void (* volatile _jmp_keep)(void);
void (*door_entry_ptr)(void) = door_entry;
int main(void) {
    _jmp_keep = door_jmp_hdr;
    _jmp_keep();
    door_entry_ptr();
    return (int)_door_magic[0];
}

/* Author code/data/bss after this point land IN the door image. */
#pragma code(door_code)
#pragma data(door_data)
#pragma bss(door_bss)

#endif /* T64_DOOR_CRT_H */
