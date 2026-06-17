/* Door runtime: SDK wrappers, DCB handshake, loader/validator. RESIDENT. */
#include "bbs/doors.h"
#include "bbs/door_abi.h"
#include "bbs/net.h"
#include "bbs/sysop.h"
#include "bbs/cfg.h"
#include "bbs/overlay.h"
#include <c64/kernalio.h>
#include <string.h>
#include <stdio.h>

#define DCB ((volatile u8 *)BBS_DCB_ADDR)
#define DOOR_ENTRY 0x9700u

/* Spike-confirmed (Task 0): a door is a separate native Oscar64 build that
 * clobbers the shared Oscar64 ZP runtime window ($02-$8F) while it runs, so
 * door_run saves/restores that range around the call. zp_buf MUST live outside
 * $02-$8F (the linker places this global well above ZP). The wrappers and the
 * entry helper MUST be #pragma native so the native door can call them and so
 * the save/restore compiles to a clean A/X-indexed copy that cannot
 * self-corrupt. Verify both in the build step. */
#define ZP_SAVE_LO  0x02u
#define ZP_SAVE_LEN 0x8Eu               /* $02..$8F inclusive = 142 bytes */
static u8 zp_buf[ZP_SAVE_LEN];

static session_t *g_door_sess;   /* active session for the wrappers */

static void w_print(const char *s)            { session_emit(g_door_sess, s); }
static void w_print_n(const char *b, u8 n)    { char t[81]; if(n>80)n=80; memcpy(t,b,n); t[n]=0; session_emit(g_door_sess, t); }
static void w_display(u8 cat, const char *nm) { session_display_file(g_door_sess, (char)cat, nm); }
static void w_clear(void)                     { session_clear_screen(g_door_sess); }
static u8   w_getkey(void)                    { u8 c; return sess_read_key(g_door_sess, &c) ? c : 0; }
static i8   w_read_line(char *b, u8 max)      { if(!sess_carrier_ok(g_door_sess)) return -1; sess_read_line(g_door_sess, b, max, FALSE); return (i8)strlen(b); }
static void w_get_caller(bbs_caller_t *o) {
  memset(o, 0, sizeof(*o));
  strncpy(o->handle,    g_door_sess->handle,        sizeof(o->handle)    - 1);
  o->access_level = g_door_sess->user.access_level;
  strncpy(o->firstname, g_door_sess->reg_firstname, sizeof(o->firstname) - 1);
  strncpy(o->lastname,  g_door_sess->reg_lastname,  sizeof(o->lastname)  - 1);
  strncpy(o->location,  g_door_sess->reg_location,  sizeof(o->location)  - 1);
  o->term_width = g_door_sess->term_width;
  o->term_mode  = (u8)g_door_sess->term_mode;
}

static const bbs_api_t g_api = {
  BBS_ABI_VERSION, w_print, w_print_n, w_display, w_clear,
  w_getkey, w_read_line, w_get_caller
};

/* The native door calls these via the API pointers — force them native. */
#pragma native(w_print)
#pragma native(w_print_n)
#pragma native(w_display)
#pragma native(w_clear)
#pragma native(w_getkey)
#pragma native(w_read_line)
#pragma native(w_get_caller)

/* enter_door: save the Oscar64 ZP runtime window, JSR the door at $9700 (a
 * direct ((void(*)(void))0x9700)() cast CRASHES oscar64 — use inline asm),
 * then restore ZP so the suspended BBS resumes cleanly. Hand __asm X-indexed
 * copy uses only A and X (no ZP scratch) — avoids self-corruption that a C
 * loop would cause by clobbering its own bytecode runtime state mid-loop. */
static void enter_door(void) {
  __asm {
    ldx #0
  ed_save:
    lda $02,x
    sta zp_buf,x
    inx
    cpx #ZP_SAVE_LEN
    bne ed_save
  }
  __asm { jsr $9700 }
  __asm {
    ldx #0
  ed_restore:
    lda zp_buf,x
    sta $02,x
    inx
    cpx #ZP_SAVE_LEN
    bne ed_restore
  }
}
#pragma native(enter_door)

/* door_run must NOT be inlined: action_doors_menu lives in OVL_DOORS ($9700),
 * and krnio_load inside door_run overwrites that region.  If door_run were
 * inlined into action_doors_menu the load would clobber its own running code.
 * After enter_door() returns (door exited), we reload OVL_DOORS so that the
 * return address back into action_doors_menu is valid.  All early-exit paths
 * (load failure, ABI mismatch) also reload because the door file load may have
 * already partially overwritten the overlay. */
__noinline void door_run(session_t *s, const door_record_t *rec) {
  char name[20];
  const volatile u8 *hdr = (const volatile u8 *)DOOR_ENTRY;

  g_door_sess = s;
  DCB[BBS_DCB_MAGIC0] = BBS_DOOR_MAGIC0;
  DCB[BBS_DCB_MAGIC1] = BBS_DOOR_MAGIC1;
  DCB[BBS_DCB_VER]    = BBS_ABI_VERSION;
  DCB[BBS_DCB_PTR_LO] = (u8)((u16)&g_api & 0xFF);
  DCB[BBS_DCB_PTR_HI] = (u8)((u16)&g_api >> 8);

  sprintf(name, "%u:%s", (unsigned)rec->drive, rec->filename);
  krnio_setnam(name);
  if (!krnio_load(1, rec->device, 1)) {
    session_emit(s, "\r\nDOOR LOAD FAILED.\r\n");
    goto reload_ovl;
  }
  if (!door_abi_check(hdr[BBS_DOOR_HDR_MAGIC], hdr[BBS_DOOR_HDR_MAGIC+1],
                      hdr[BBS_DOOR_HDR_VER])) {
    session_emit(s, "\r\nDOOR ABI MISMATCH.\r\n");
    goto reload_ovl;
  }
  enter_door();                      /* save ZP, JSR $9700, restore ZP */

reload_ovl:
  /* Reload OVL_DOORS so action_doors_menu's code is back at its load address
   * before we return into it.  The door load (and the failed-load path above)
   * clobbered $9700-$BFFF.  Mark WFC displaced too — it needs reload later. */
  krnio_setnam(P"OVL_DOORS");
  krnio_load(1, bbs_cfg.device_system, 1);
  wfc.ovl_wfc_loaded = FALSE;
}

/* ── OVL_DOORS overlay: door menu UI ────────────────────────────────────────
 * action_doors_menu lives in the doors overlay so it doesn't consume resident
 * space.  door_run (above) reloads OVL_DOORS before returning here, so this
 * code is valid at the return address despite the door having overwritten
 * $9700-$BFFF during its execution. */
#pragma code(doors_code)
#pragma data(doors_data)
#pragma bss(doors_bss)

void action_doors_menu(session_t *s) {
  door_record_t d;
  u8 i, key;
  session_emit(s, "\r\nDOOR PROGRAMS\r\n\r\n");
  for (i = 1; i <= DOORS_MAX; i++) {
    if (door_by_id(i, &d, bbs_cfg.device_system) != BBS_OK) continue;
    if (!door_visible(&d, s->user.access_level)) continue;
    { char line[40]; sprintf(line, " [%c] %s\r\n", d.cmd_key, d.title);
      session_emit(s, line); }
  }
  session_emit(s, "\r\nSELECT (RETURN=BACK): ");
  { u8 c; if (!sess_read_key(s, &c)) return; key = c; }
  if (key == '\r' || key == '\n') { s->menu_displayed = FALSE; return; }
  if (door_by_key((char)key, &d, bbs_cfg.device_system) == BBS_OK
      && door_visible(&d, s->user.access_level)) {
    door_run(s, &d);
  }
  s->menu_displayed = FALSE;
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)

void session_run_login_doors(session_t *s) {
  (void)s;
  /* stub: run any doors flagged for login execution (future task) */
}
