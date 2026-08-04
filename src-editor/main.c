#include <c64/vic.h>
#include <c64/kernalio.h>
#include <conio.h>
#include <stdio.h>
#include <ctype.h>
#include "bbs/version.h"
#include "bbs/config.h"
#include "bbs/types.h"
#include "bbs/cfg.h"
#include "bbs/err.h"
#include "bbs/users.h"
#include "bbs/boards.h"
#include "bbs/file_areas.h"
#include "bbs/votes.h"
#include "setup.h"
#include "admin/admin.h"
#include "ui/ui.h"
#include "bbs/hal/disk.h"
#include "bbs/hal/reu.h"
#include "bbs/callers.h"
#include "bbs/sstatus.h"
#include "bbs/syscnt.h"

/* Code/data/BSS in $0880-$BFFF (BASIC ROM banked out via $01=$36 in main),
 * stack in $C000-$CFFF.
 * msgs_code/msgs_data/msgs_bss are declared so messages.c/#pragma code(msgs_code)
 * compiles cleanly; they're added to the main region so their content folds in. */
#pragma section( msgs_code, 0 )
#pragma section( msgs_data, 0 )
#pragma section( msgs_bss,  0, , , bss )
#pragma region( main, 0x0880, 0xC000, , , {code, data, bss, heap, msgs_code, msgs_data, msgs_bss} )
#pragma region( cstk, 0xC000, 0xD000, , , {stack} )
#pragma heapsize(0)

/* CONFIGURE SysOp Editor — disk init, user/msg/file/vote/config management. */

/** Exit the editor by jumping to the KERNAL reset vector instead of RTSing
 * back to BASIC. CONFIGURE's BSSEnd sits inside the BASIC ROM window
 * ($A000-$BFFF, banked out at startup so the linker can place BSS there —
 * see the #pragma region above and the $01=$36 write in main()). After a
 * LOAD, BASIC's own variable/workspace pointers land just past the ~43KB
 * program image (around $B0D0), inside that same window. A plain RTS-back-
 * to-BASIC only restores $01=$37 and resumes BASIC's EXISTING interpreter
 * state, which read as ROM (not RAM) for the whole run and is now corrupt —
 * observed on hardware as an infinite "?FORMULA TOO COMPLEX" storm after
 * "GOODBYE.". Jumping to the hardware reset entry point ($FCE2, same as the
 * KERNAL RESET vector at $FFFC/$FFFD) instead performs a full BASIC cold
 * start, which reinitializes those pointers from scratch rather than
 * resuming corrupted ones, sidestepping the problem entirely.
 *
 * Must bank in $01=$37 (BASIC+KERNAL ROM+I/O, the hardware default) first:
 * $FCE2 ends with JMP ($A000), reading BASIC's cold-start vector out of
 * BASIC ROM — with ROM still banked out that read would hit our BSS instead.
 * krnio_clrchn() first clears any KERNAL input/output channel selection
 * left over from disk I/O (main_menu()'s admin submenus, or cfg_init()'s
 * CONFIG read on the failure path below) so the reset starts from a clean
 * KERNAL channel state, same as a real RESET would find after power-on.
 * The jump never returns.
 *
 * Written as inline asm (JMP $FCE2), not a `((void(*)(void))0xFCE2)()`
 * function-pointer call: the function-pointer form crashes the oscar64
 * compiler outright (segfault, reproduced standalone) rather than emitting
 * bad code — confirmed this is the correct absolute JMP either way, but the
 * asm form is what actually compiles. */
static void editor_exit_to_reset(void)
{
    krnio_clrchn();
    *((volatile char *)0x01) = 0x37;
    __asm {
        jmp $fce2
    }
}

static void do_stats(void)
{
    ui_screen_header("BBS STATISTICS");
    printf("BBS:        %s\n", bbs_cfg.bbs_name);
    printf("SYSOP:      %s\n", bbs_cfg.sysop_name);
    printf("\n");
    printf("USERS:      %u\n", (unsigned)user_count(bbs_cfg.device_system));
    printf("MSG AREAS:  %u\n", (unsigned)board_count(bbs_cfg.device_msgs));
    printf("FILE AREAS: %u\n", (unsigned)file_area_count(bbs_cfg.device_files));
    printf("POLLS:      %u\n", (unsigned)vote_count(bbs_cfg.device_msgs));
    printf("\n");
    ui_press_any_key();
}

/** Seed the CALLERS sequential file with one SYSOP entry.
 * Format: 41-char line "MM/DD HH:MM A HHHHHHHHHHHHHH BBBBB DDDDD" + CR
 * Written at INIT so the WFC callers section always has something to show. */
static void callers_init(u8 device)
{
    /* Seed entry: date 01/01, time 12:00 AM, handle SYSOP, baud/dur both 0 */
    static const char seed[] =
        "01/01 12:00 A SYSOP           00000 00000";
    if (cfg_send_drive_init(device, bbs_cfg.init_system) != BBS_OK) return;
    disk_scratch(device, bbs_cfg.drive_system, CALLERS_FILE);
    if (disk_open(device, bbs_cfg.drive_system,
                  CALLERS_FILE, DISK_WRITE) != BBS_OK) return;
    disk_putline(seed);
    disk_close();
}

static void do_init_disk(u8 device)
{
    bbs_err_t err;
    char ch;

    ui_screen_header("INITIALIZE FILES?");
    printf("THIS WILL WIPE THE USER LOG\n");
    printf("AND CANNOT BE UNDONE.\n");
    printf("\n");
    ui_hotkey_label('Y', "PROCEED");
    ui_hotkey_label('N', "CANCEL");
    printf("\n\n");
    printf("CMD?:");
    ch = (char)toupper((unsigned char)getch());
    printf("\n");
    if (ch != 'Y') return;

    err = setup_create_user_database(device);
    if (err != BBS_OK) {
        ui_op_error("INIT USR LOG", (u8)err);
        return;
    }
    err = setup_create_user_profiles(device);
    if (err != BBS_OK) {
        ui_op_error("INIT USR PROF", (u8)err);
        return;
    }
    err = setup_create_access_levels(device);
    if (err != BBS_OK) {
        ui_op_error("INIT ACCESS", (u8)err);
        return;
    }
    printf("ACCESS LEVELS CREATED.\n");
    callers_init(device);
    printf("\nCALLERS LOG CREATED.\n");
    sstatus_save("");      /* empty status -> default "PLAYING ATARI 2600" until set */
    syscnt_init(device);   /* zeroed counters, resets on first boot */
    printf("STATUS + COUNTERS CREATED.\n");
    ui_press_any_key();
}

static void main_menu(void)
{
    static const ui_menu_item_t items[] = {
        { 'I', "INIT BBS"       },
        { 'M', "MSG BOARDS"     },
        { 'U', "USER MGMT"      },
        { 'F', "FILE AREAS"     },
        { 'V', "VOTE MGMT"      },
        { 'D', "DOOR PROGRAMS"  },
        { 'C', "CONFIG OPTIONS" },
        { 'S', "STATISTICS"     },
        { 'Q', "QUIT"           },
    };

    for (;;) {
        char ch;
        ui_menu_display("MAIN MENU", items, 9);
        ch = ui_menu_input("CHOICE:", "IUMFVDCSQ");
        switch (ch) {
            case 'I': do_init_disk(bbs_cfg.device_system);        break;
            case 'U': admin_users_menu(bbs_cfg.device_system);    break;
            case 'M': admin_messages_menu(bbs_cfg.device_msgs);   break;
            case 'F': admin_files_menu(bbs_cfg.device_files);     break;
            case 'V': admin_votes_menu(bbs_cfg.device_msgs);      break;
            case 'D': admin_doors_menu(bbs_cfg.device_doors);     break;
            case 'S': do_stats();                                  break;
            case 'C': admin_config_menu(bbs_cfg.device_system);   break;
            case 'Q': return;
            default:  break;
        }
    }
}

int main(void)
{
    bbs_err_t err;

    /* Bank out BASIC ROM so BSS placed at $A000-$BFFF by linker is accessible */
    *((volatile char *)0x01) = 0x36;

    vic.color_border = COLOR_GREEN;
    vic.color_back   = COLOR_BLACK;

    clrscr();
    /* Lowercase/text charset + PETSCII<->ASCII translation, so the editor can
     * accept and display mixed-case text. Uppercase ASCII chrome still renders
     * uppercase; unshifted keys yield lowercase, Shift yields uppercase. */
    iocharmap(IOCHM_PETSCII_2);
    printf("INITIALIZING...\n");

    err = cfg_init();
    if (err != BBS_OK && err != BBS_ENOTFOUND) {
        printf("ERROR: CONFIG INIT FAILED\n");
        /* bbs_cfg.init_system/drive_system are still their pre-cfg_init()
         * defaults here, so disk_reset_cursor_root() is a correct no-op in
         * both builds (see src/main.c's matching early exit and disk.c's
         * doc comment on disk_reset_cursor_root()). Called anyway so this
         * exit doesn't silently rot if that ever changes.
         *
         * #ifndef'd out under T64_STORE_SEQ: CONFIGURE-SIEC has only 47
         * bytes of BSS headroom (measured from BSSEnd in the .map — see the
         * other call site below for the full story) and neither call fits.
         * REL CONFIGURE has ~2346 bytes free and gets the fix; SIEC
         * CONFIGURE keeps its pre-existing stranded-cursor bug rather than
         * dropping a feature to make room (standing project constraint). */
#ifndef T64_STORE_SEQ
        disk_reset_cursor_root(bbs_cfg.device_system);
#endif
        /* A sysop whose config fails to load needs a usable prompt just as
         * much as a normal QUIT — hold the error on screen for a keypress
         * before editor_exit_to_reset() clears it (see that function's
         * doc comment for why RTS-to-BASIC is not safe here). */
        ui_press_any_key();
        editor_exit_to_reset();
        return 1;
    }

#ifdef T64_STORE_SEQ
    /* rel_open() (rel_seq.c) hard-requires a working REU — without this call
     * bbs_cfg.reu_enabled stays FALSE all run and every database operation
     * (INIT BBS, user/board/file/vote edits) fails with BBS_EIO. */
    {
        u16 reu_sz = reu_detect();
        if (reu_sz == 0) {
            printf("WARNING: NO REU DETECTED\n");
            printf("DATABASE ACCESS WILL FAIL\n");
        } else if (reu_sz >= 1024) {
            printf("REU: %u MB\n", (unsigned)(reu_sz >> 10));
        } else {
            printf("REU: %u KB\n", reu_sz);
        }
    }
#endif

    main_menu();

    krnio_clrchn();

    clrscr();
    printf("GOODBYE.\n");

    /* main_menu()'s admin submenus can leave the drive cursor anywhere a
     * msgs/files/doors op last parked it — same stranded-cursor hazard
     * documented on disk_reset_cursor_root() (src/hal/disk.c), and the exact
     * bug reported on hardware: exiting REL CONFIGURE left a physical
     * sd2iec on partition 2 while the install lived in partition 1.
     *
     * #ifndef'd out under T64_STORE_SEQ: adding both call sites in this file
     * grows CONFIGURE-SIEC's code enough to push BSSStart 79 bytes later —
     * code and bss share one region (see this file's #pragma region), and
     * the calls also pull in disk_reset_cursor_root()'s SEQ body itself,
     * previously dead-stripped here since nothing in the editor called it.
     * 79 bytes blows through the build's 47-byte headroom: oscar64 fails to
     * link with "Could not place object 's_lflag' — Size 32 Available 0",
     * an exact 32-byte shortfall confirmed with `-rmp`'s error map (total
     * requested bss is unchanged at 1811 bytes; only BSSStart moved).
     * Left unmodified per the no-forced-fit / nothing-gets-dropped
     * constraint — CONFIGURE-SIEC keeps the pre-existing stranded-cursor bug
     * rather than losing a feature to make room. */
#ifndef T64_STORE_SEQ
    disk_reset_cursor_root(bbs_cfg.device_system);
#endif

    /* Hold GOODBYE. on screen for a keypress — editor_exit_to_reset()'s
     * reset jump clears the screen almost immediately (KERNAL CINT), so
     * without this pause the message would be unreadable. See that
     * function's doc comment for why a plain RTS back to BASIC is not
     * safe here instead. */
    ui_press_any_key();
    editor_exit_to_reset();

    return 0;
}
