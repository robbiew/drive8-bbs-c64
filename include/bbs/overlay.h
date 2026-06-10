#ifndef INCLUDE_BBS_OVERLAY_H
#define INCLUDE_BBS_OVERLAY_H

/**
 * MSGS overlay section declarations.
 *
 * Include this header in every source file that belongs to the MSGS overlay
 * (bulletin.c, messages.c, editor.c, usrptr.c) BEFORE the
 *   #pragma code(msgs_code) / #pragma data(msgs_data) / #pragma bss(msgs_bss)
 * directives.  Oscar64 requires section names to be declared in the same
 * translation unit that references them via #pragma code/data/bss.
 *
 * The matching #pragma region and #pragma overlay live in src/main.c.
 */
#pragma section( msgs_code, 0 )
#pragma section( msgs_data, 0 )
#pragma section( msgs_bss,  0, , , bss )

/* WFC overlay sections — included by sysop.c before #pragma code(wfc_code) */
#pragma section( wfc_code, 0 )
#pragma section( wfc_data, 0 )
#pragma section( wfc_bss,  0, , , bss )

#endif /* INCLUDE_BBS_OVERLAY_H */
