/* Host unit tests for the menu garbage-flood guard (menu_dispatch). */
#include "host.h"
#include "bbs/menu.h"
#include "bbs/session.h"

/* ── Stubs for session symbols referenced by menu.c ── */
static char last_emit[64];
void session_emit(const session_t *s, const char *text) {
    (void)s;
    strncpy(last_emit, text, sizeof(last_emit) - 1);
    last_emit[sizeof(last_emit) - 1] = '\0';
}
void session_clear_screen(const session_t *s) { (void)s; }
bbs_err_t session_display_file(const session_t *s, char prefix, const char *base) {
    (void)s; (void)prefix; (void)base;
    return BBS_ENOTFOUND;
}
void sess_reset_color(const session_t *s) { (void)s; }

/* ── Minimal menu table ── */
static int action_hits = 0;
static void stub_action(session_t *s) { (void)s; action_hits++; }

static menu_cmd_t main_cmds[] = {
    { 'M', "MESSAGES", "", 0, TRUE, stub_action },
};
menu_def_t menus[] = {
    { "main", "MAIN MENU", "main", main_cmds, 1 },
};
u8 menu_count = 1;

static void fresh_session(session_t *s, bool_t is_local) {
    memset(s, 0, sizeof(*s));
    s->state = SESS_IN_MENU;
    s->is_local = is_local;
    strcpy(s->menu_state.current_menu, "main");
}

int main(void) {
    session_t s;
    u8 i;

    /* Below the cap: unknown commands just complain, session stays up */
    fresh_session(&s, FALSE);
    for (i = 0; i < MENU_GARBAGE_LIMIT - 1; i++) menu_dispatch(&s, 'Z');
    EXPECT_EQ("below_cap.state", s.state, SESS_IN_MENU);
    EXPECT_MEM("below_cap.msg", last_emit, "\r\nUNKNOWN", 9);

    /* At the cap: session is forced to logoff */
    menu_dispatch(&s, 'Z');
    EXPECT_EQ("at_cap.state", s.state, SESS_LOGOFF);

    /* Valid commands do NOT reset the count: an echo feedback loop reflects
     * the BBS's own reply text, which contains valid command letters */
    fresh_session(&s, FALSE);
    for (i = 0; i < MENU_GARBAGE_LIMIT - 1; i++) menu_dispatch(&s, 'Z');
    menu_dispatch(&s, 'M');
    EXPECT_EQ("valid_no_reset.action", action_hits, 1);
    EXPECT_EQ("valid_no_reset.state", s.state, SESS_IN_MENU);
    menu_dispatch(&s, 'Z');
    EXPECT_EQ("valid_no_reset.trip", s.state, SESS_LOGOFF);

    /* Local sysop sessions are exempt — no modem to wedge */
    fresh_session(&s, TRUE);
    for (i = 0; i < 2 * MENU_GARBAGE_LIMIT; i++) menu_dispatch(&s, 'Z');
    EXPECT_EQ("local_exempt.state", s.state, SESS_IN_MENU);

    return test_summary("menu_garbage");
}
