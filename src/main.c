/**
 * TURBO/64 BBS — Main Entry Point and Kernel Loop
 *
 * Boot sequence:
 *  1. Load configuration (config file)
 *  2. Initialize modem (ACIA)
 *  3. Display SysOp local status
 *  4. Main loop: wait for carrier → new session → logoff → repeat
 */

#include <stdio.h>
#include <string.h>

/* Oscar64 headers */
#include <c64/vic.h>

/* BBS headers */
#include "bbs/version.h"
#include "bbs/config.h"
#include "bbs/types.h"
#include "bbs/err.h"
#include "bbs/cfg.h"
#include "bbs/net.h"
#include "bbs/session.h"
#include "bbs/menu.h"
#include "bbs/sysop.h"
#include "bbs/callers.h"
#include "bbs/rel.h"
#include "bbs/records.h"
#include "bbs/users.h"
#include "bbs/hal/disk.h"
#include "bbs/hal/term.h"
#include "bbs/hal/clock.h"
#include "bbs/hal/reu.h"

/* Core region: code/data/BSS/heap in $0880-$9700 (~36.4 KB).
 * Overlay zone at $9700-$BFFF holds the MSGS/WFC overlays (10,496 bytes).
 * BASIC ROM ($A000-$BFFF) is banked out in main() so those addresses are RAM.
 * Stack lives in always-RAM at $C000-$D000. $C000-$C3FF (bottom 1KB) is also
 * used as VIC screen RAM during 80-col spy sessions; stack grows down from $CFFF
 * and session call depth stays well under 3KB so there is no overlap at runtime.
 * Boundary: $9D80 → $9B00 (reading loop) → $9800 (msg scan) → $9700 (subj[16]). */
#pragma region( main, 0x0880, 0x9700, , , {code, data, bss, heap} )
#pragma region( cstk, 0xC000, 0xD000, , , {stack} )
#pragma heapsize(0)

/* MSGS overlay: bulletin + messages + editor loaded on demand from disk.
 * $9700-$C000 = 10,496 bytes.
 * NEARLY FULL as of v0.1.0 (<100 bytes) — after touching bulletin/messages/
 * editor/usrptr, check the `regions` table in build/c64/BOOT-*.map.
 * Bank ID 1 → file "OVL_MSGS" on disk (oscar64: pragma name uppercased). */
#pragma overlay( ovl_msgs, 1 )
#pragma section( msgs_code, 0 )
#pragma section( msgs_data, 0 )
#pragma section( msgs_bss,  0, , , bss )
#pragma region( msgs, 0x9700, 0xC000, , 1, {msgs_code, msgs_data, msgs_bss} )

/* WFC overlay: all WFC draw functions, wfc_display/update/init, RTC helpers.
 * Bank 2 — same address zone as MSGS; the two are mutually exclusive at runtime.
 * Loaded at boot and reloaded after any session where OVL_MSGS displaced it. */
#pragma overlay( ovl_wfc, 2 )
#pragma section( wfc_code, 0 )
#pragma section( wfc_data, 0 )
#pragma section( wfc_bss,  0, , , bss )
#pragma region( wfc, 0x9700, 0xC000, , 2, {wfc_code, wfc_data, wfc_bss} )

/* BOOT overlay: config-load code (cfg_init + its parse helpers) runs once at
 * boot and is then dead weight in the resident region.  Bank 3 — same address
 * zone as MSGS/WFC; boot strictly precedes any session, so wfc_init/the first
 * session freely overwrites it.  Frees ~1.7 KB of the cramped main region. */
#pragma overlay( ovl_boot, 3 )
#pragma section( boot_code, 0 )
#pragma section( boot_data, 0 )
#pragma section( boot_bss,  0, , , bss )
#pragma region( boot, 0x9700, 0xC000, , 3, {boot_code, boot_data, boot_bss} )

/* DOORS overlay: door menu UI (action_doors_menu).  Bank 4 — same address zone.
 * door_run + wrappers + enter_door stay RESIDENT because door_run's krnio_load
 * overwrites $9700-$BFFF (this overlay's code), so the caller must not be in
 * the overlay while the door is running.  door_run reloads OVL_DOORS after
 * enter_door() returns, before returning to action_doors_menu's call site. */
#pragma overlay( ovl_doors, 4 )
#pragma section( doors_code, 0 )
#pragma section( doors_data, 0 )
#pragma section( doors_bss,  0, , , bss )
#pragma region( doors, 0x9700, 0xC000, , 4, {doors_code, doors_data, doors_bss} )

/* FILES overlay: file list, Punter protocol send/receive.  Bank 5.
 * xfer_punter_send/recv stay RESIDENT (they call krnio_load which overwrites
 * this zone); files_run and the Punter implementation live here. */
#pragma overlay( ovl_files, 5 )
#pragma section( files_code, 0 )
#pragma section( files_data, 0 )
#pragma section( files_bss,  0, , , bss )
#pragma region( files, 0x9700, 0xC000, , 5, {files_code, files_data, files_bss} )

/* ZMODEM overlay: bank 6.  Loaded on demand by xfer_zmodem_send/recv in
 * the resident xfer.c shim; never called from within another overlay. */
#pragma overlay( ovl_zmodem, 6 )
#pragma section( zmodem_code, 0 )
#pragma section( zmodem_data, 0 )
#pragma section( zmodem_bss,  0, , , bss )
#pragma region( zmodem, 0x9700, 0xC000, , 6, {zmodem_code, zmodem_data, zmodem_bss} )

/**
 * main_print()
 *
 * Local printf that works with Oscar64 runtime.
 */
static void main_print(const char *text) {
  printf("%s", text);
}

/**
 * main_print_upper()
 *
 * Print to the local uppercase/graphics console, forcing ASCII letters to
 * uppercase. Config-identity strings (BBS name, sysop name, sysop status) are
 * stored mixed-case for caller-facing output, but the local console charset
 * has no lowercase glyphs — lowercase bytes would render as graphics junk.
 */
static void main_print_upper(const char *text) {
  char c;
  while ((c = *text++) != 0) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 0x20);
    putchar(c);
  }
}

/**
 * boot_sequence()
 *
 * Initialize BBS on startup.
 *
 * Returns:
 *   BBS_OK     — ready to accept calls
 *   BBS_EFATAL — fatal initialization error
 */
static bbs_err_t boot_sequence(void) {
  bbs_err_t err;

  /* Clear screen, set colors */
  vic.color_border = 0;  /* Black border */
  vic.color_back = 0;    /* Black background */

  main_print("\x93\x8e");  /* PETSCII: CLR/HOME, uppercase/graphics mode (default) */
  main_print("\x05\x9e");  /* Light cyan text */

  main_print("TURBO/64 BBS V");
  main_print(BBS_RELEASE_VERSION_COMPACT);
  main_print("\n\n");

  /* Load configuration */
  main_print("LOADING SETUP...\n");
  err = cfg_init();
  if (err != BBS_OK && err != BBS_ENOTFOUND) {
    main_print("ERROR: CONFIG LOAD FAILED\n");
    return BBS_EFATAL;
  }
  main_print("  BBS: ");
  main_print_upper(bbs_cfg.bbs_name);
  main_print("\n");
  main_print("  SYSOP: ");
  main_print_upper(bbs_cfg.sysop_name);
  main_print("\n");
  if (cfg_send_drive_init(bbs_cfg.device_system, bbs_cfg.init_system) != BBS_OK) {
    main_print("ERROR: SYSTEM DRIVE INIT FAILED\n");
    return BBS_EFATAL;
  }

  /* Initialize modem (ACIA/SwiftLink) */
  main_print("\nINIT MODEM...\n");
  err = net_init();
  if (err != BBS_OK) {
    main_print("ERROR: MODEM INIT FAILED\n");
    return BBS_EFATAL;
  }
  {
    u8 acia = net_acia_status();
    printf("  ACIA STATUS: $%02X\n", (unsigned)acia);
    /* $10 = TX empty (normal idle). Bit 6=0 means DSR active. */
    main_print(((acia & 0x40) != 0) ? "  DSR: INACTIVE\n" : "  DSR: ACTIVE!\n");
  }
  main_print("  DEVICE: ");
  if (bbs_cfg.device_system != bbs_cfg.device_msgs) {
    printf("SYSTEM=%u/%u MSGS=%u/%u",
           bbs_cfg.device_system, bbs_cfg.drive_system,
           bbs_cfg.device_msgs, bbs_cfg.drive_msgs);
  } else {
    printf("DEFAULT=%u/%u", bbs_cfg.device_system, bbs_cfg.drive_system);
  }
  main_print("\n");

  /* Detect REU (RAM Expansion Unit) if present */
  main_print("\nCHECKING REU...\n");
  {
    /* reu_detect() already records size + enabled in bbs_cfg; it returns the
     * detected size in KB (0 = absent). */
    u16 reu_sz = reu_detect();
    if (reu_sz == 0) {
      main_print("  REU: NOT FOUND\n");
    } else if (reu_sz >= 1024) {
      printf("  REU: %u MB\n", (unsigned)(reu_sz >> 10));  /* /1024 via shift */
    } else {
      printf("  REU: %u KB\n", reu_sz);
    }
  }

  /* Check if USR LOG file exists and has data */
  main_print("\nCHECKING USR LOG...\n");
  {
    rel_handle_t h;
    bbs_err_t check = rel_open(bbs_cfg.device_system, bbs_cfg.drive_system,
                                "USR LOG", RECORD_SIZE_USER, &h);
    if (check == BBS_OK) {
      /* Verify file has data by attempting to read record 1 (SYSOP) */
      u8 buf[RECORD_SIZE_USER];
      u8 got = 0;
      check = rel_position(h, 1);
      if (check == BBS_OK) {
        check = rel_read(h, buf, RECORD_SIZE_USER, &got);
      }
      rel_close(h);
      /* Verify record 1 has SYSOP user (ID=1, not 0 which means empty) */
      if (check == BBS_OK && got > 0 && buf[0] == 1) {
        main_print("  USR LOG: OK\n");
        /* Load the user-record cache into REU (robust file-static DMA path);
         * report whether it's serving from REU or falling back to disk. */
        user_cache_load(bbs_cfg.device_system);
        main_print(user_cache_active() ? "  USER CACHE: ON (REU)\n"
                                       : "  USER CACHE: OFF (DISK)\n");
      } else {
        main_print("  USR LOG: EMPTY\n");
        /* Clean up the empty file that was auto-created by rel_open */
        disk_scratch(bbs_cfg.device_system, bbs_cfg.drive_system, "USR LOG");
        main_print("\nERROR: USR LOG FILE NOT INITIALIZED\n");
        main_print("RUN CONFIGURE-");
        main_print(BBS_RELEASE_VERSION_COMPACT);
        main_print(".PRG TO INITIALIZE\n");
        main_print("THE USER DATABASE BEFORE RUNNING BOOT.\n");
        return BBS_EFATAL;
      }
    } else {
      main_print("  USR LOG: NOT FOUND\n");
      main_print("\nERROR: USR LOG FILE NOT FOUND\n");
      main_print("RUN CONFIGURE-");
      main_print(BBS_RELEASE_VERSION_COMPACT);
      main_print(".PRG TO INITIALIZE\n");
      main_print("THE USER DATABASE BEFORE RUNNING BOOT.\n");
      return BBS_EFATAL;
    }
  }

  main_print("\nCHECKING FOR REAL TIME CLOCK...\n");

  /* Initialize menu system */
  err = menu_init();
  if (err != BBS_OK) {
    main_print("ERROR: MENU SYSTEM INIT FAILED\n");
    return BBS_EFATAL;
  }

  /* Initialize WFC state (clock + log buffer) */
  wfc_init();

  return BBS_OK;
}

/**
 * main_loop()
 *
 * WFC-driven BBS main loop.
 *
 * Loop:
 *   1. Display C*BASE-style WFC screen
 *   2. Poll modem + keyboard via wfc_update()
 *   3. On carrier: run session
 *   4. After session: log to activity buffer, redraw WFC
 */
static void main_loop(void) {
  session_t session;
  // cppcheck-suppress variableScope
  bbs_err_t err;
  u16 got;
  clock_tod_t sess_start;
  u16 elapsed;

  wfc_display();

  /* Arm the Timer-B RX interrupt now that boot disk I/O is done, so the
   * inbound CONNECT result code is captured without single-byte overrun. */
  net_irq_arm();

  for (;;) {
    /* Poll modem + keyboard; updates time field each second */
    err = wfc_update();

    if (err == BBS_EQUIT) {
      break;  /* graceful exit — returns to BASIC */
    }

    if (err == BBS_EAGAIN) {
      /* ── New caller detected (modem or local sysop logon) ── */
      {
        bool_t local = wfc.local_logon;
        wfc.local_logon = FALSE;   /* consume flag */
        err = session_init(&session, local);
      }
      if (err != BBS_OK) {
        wfc_display();   /* redraw and keep waiting */
        continue;
      }

      /* ── Terminal detection for remote callers (first connection) ── */
      clock_read(&sess_start);
      if (!session.is_local) {
        term_detect_all(&session);
      }
      /* PETSCII sessions run in lowercase/text charset so mixed-case content
       * renders correctly. Detection's g.login display may have left graphics mode. */
      if (session.petscii_lower) {
        session_emit_charset(&session, 0x0Eu);
      }
      session_spy_init(&session);

      /* Display WFC overlay screen during session */
      wfc_display_session(&session);

      /* Run session until logoff or carrier drop */
      while (session.state != SESS_IDLE && session.state != SESS_ERROR) {
        if (!session.is_local) {
          net_rx((void *)0, 0, &got);
          if (net_state() != NET_CONNECTED) {
            break;
          }
        }
        err = session_step(&session);
        if (err != BBS_OK) {
          session.state = SESS_ERROR;
          session.error = err;
        }

        /* Resident SysOp tick: action keys + 80-col status line (once/second). */
        session_spy_poll();

        /* PETSCII 40-col spy panel (WFC overlay; 80-col handled by poll above). */
        {
          clock_tod_t sess_now;
          u16 sess_elapsed;
          clock_read(&sess_now);
          sess_elapsed = clock_elapsed(&sess_start, &sess_now);
          wfc_update_session(&session, sess_elapsed);
        }
      }

      /* Log caller — only once a real user has authenticated (user_id != 0).
       * Callers who hang up before login, and bots that connect without logging
       * in, never get an entry, so they don't clutter the WFC activity log or
       * the on-disk CALLERS file (and a half-typed/garbage handle like a leaked
       * "NO CARRIER" string is never recorded). */
      if (session.user_id != 0) {
        clock_tod_t sess_end;
        clock_read(&sess_end);
        elapsed = clock_elapsed(&sess_start, &sess_end);
        wfc_log_session(session.handle, bbs_cfg.baud_rate, elapsed);
        callers_log(&session, elapsed);
      }

      session_done(&session);
      if (!session.is_local) {
        net_disconnect();
        /* Pump ACIA to let NET_DROPPING → NET_IDLE settle */
        net_rx((void *)0, 0, &got);
        net_rx((void *)0, 0, &got);
      }

      /* Redraw full WFC screen */
      wfc_display();
    }
  }
}

/**
 * main()
 *
 * BBS entry point.
 */
int main(void)
{
  bbs_err_t err;

  /* Bank out BASIC ROM ($A000-$BFFF) so BSS placed there by the linker is
   * accessible as RAM.  Keeps KERNAL ($E000) and I/O ($D000) active. */
  *((volatile char *)0x01) = 0x36;

  /* Boot sequence */
  err = boot_sequence();
  if (err != BBS_OK) {
    main_print("\nBOOT FAILED. HALTING.\n");
    return 1;
  }

  /* Main loop */
  main_loop();

  return 0;
}
