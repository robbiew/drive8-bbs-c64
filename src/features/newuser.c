/* TURBO/64 BBS — New user registration flow */
#include "bbs/newuser.h"
#include "bbs/session.h"
#include "bbs/auth.h"
#include "bbs/users.h"   /* USER_PASSWORD_MIN/MAX */
#include "bbs/cfg.h"
#include "bbs/term.h"
#include <string.h>
#include <stdio.h>

/* Convenience wrapper around sess_color() for inline prompt styling */
#define RC_CYAN(s)   sess_color(s, 0x9f, "\x1b[36m")
#define RC_WHITE(s)  sess_color(s, 0x05, "\x1b[37m")
#define RC_LTBLUE(s) sess_color(s, 0x9a, "\x1b[94m")
#define RC_YELLOW(s) sess_color(s, 0x9e, "\x1b[33m")
#define RC_RVON(s)   sess_color(s, 0x12, "\x1b[7m")
#define RC_RVOFF(s)  sess_color(s, 0x92, "\x1b[27m")

/* New-user registration runs only at login, before any menu, while the WFC
 * overlay is resident — so it lives in the WFC overlay to free main code space
 * for the always-resident 80-col spy render path. session_reg_step() is called
 * from session.c only during SESS_REGISTERING (MSGS overlay not yet loaded). */
#pragma code(wfc_code)
#pragma data(wfc_data)
#pragma bss(wfc_bss)

// cppcheck-suppress constParameterPointer
static u8 sess_reg_collect(session_t *s, u8 ch, char *buf, u8 max, u8 mask)
{
    u8 len = (u8)strlen(buf);
    if (ch == '\r' || ch == '\n') return 1;
    if ((ch == 0x08 || ch == 0x7F || ch == 0x14) && len > 0) {
        buf[len - 1] = 0;
        sess_erase_char(s);
    } else if (ch >= 0x20 && ch < 0x7F && len < max) {
        if (ch >= 'a' && ch <= 'z') ch -= 0x20;
        buf[len] = (char)ch;
        buf[len + 1] = 0;
        { char e[2]; e[0] = mask ? (char)'*' : (char)ch; e[1] = 0; sess_tx(e); }
    }
    return 0;
}

static const char *reg_mode_name(u8 m)
{
    if (m == TERM_ANSI_CP437)   return "ANSI/CP437";
    if (m == TERM_ASCII)        return "ASCII";
    return "PETSCII";
}

static void sess_reg_show_confirm(const session_t *s)
{
    u8 i, plen;
    char stars[12];

    /* Reverse-video header bar — ImageBBS3 style */
    RC_RVON(s); RC_CYAN(s);
    sess_tx("\r\nCONFIRM YOUR INFORMATION\r\n");
    RC_RVOFF(s);

    RC_LTBLUE(s); sess_tx("1"); RC_CYAN(s); sess_tx(". HANDLE:   ");
    RC_WHITE(s);  sess_tx(s->handle); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("2"); RC_CYAN(s); sess_tx(". PASSWORD: ");
    RC_WHITE(s);
    plen = (u8)strlen(s->password);
    if (plen > 11) plen = 11;
    for (i = 0; i < plen; i++) stars[i] = '*';
    stars[plen] = 0;
    sess_tx(stars); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("3"); RC_CYAN(s); sess_tx(". EMAIL:    ");
    RC_WHITE(s);  sess_tx(s->reg_email[0]     ? s->reg_email     : "(NONE)"); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("4"); RC_CYAN(s); sess_tx(". FIRST:    ");
    RC_WHITE(s);  sess_tx(s->reg_firstname[0] ? s->reg_firstname : "(NONE)"); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("5"); RC_CYAN(s); sess_tx(". LAST:     ");
    RC_WHITE(s);  sess_tx(s->reg_lastname[0]  ? s->reg_lastname  : "(NONE)"); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("6"); RC_CYAN(s); sess_tx(". LOCATION: ");
    RC_WHITE(s);  sess_tx(s->reg_location[0]  ? s->reg_location  : "(NONE)"); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("7"); RC_CYAN(s); sess_tx(". GRAPHICS: ");
    RC_WHITE(s);  sess_tx(reg_mode_name(s->term_mode)); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("8"); RC_CYAN(s); sess_tx(". COLUMNS:  ");
    RC_WHITE(s);  sess_tx(s->term_width == 80 ? "80" : "40"); sess_tx("\r\n");

    RC_LTBLUE(s); sess_tx("9"); RC_CYAN(s); sess_tx(". ROWS:     ");
    RC_WHITE(s);  sess_tx(s->term_rows  == 25 ? "25" : "24"); sess_tx("\r\n");
}

static void reg_start_step(session_t *s, u8 step)
{
    s->reg_step = step;
    switch (step) {
        case 1:  memset(s->handle,        0, sizeof(s->handle));
                 if (!s->reg_editing) {
                     RC_RVON(s); RC_CYAN(s);
                     sess_tx("\r\nNEW USER REGISTRATION\r\n");
                     RC_RVOFF(s);
                 }
                 RC_CYAN(s); sess_tx("\r\nHANDLE   ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 2:  memset(s->password,      0, sizeof(s->password));
                 RC_CYAN(s); sess_tx("\r\nPASSWORD ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 3:  memset(s->reg_confirm,   0, sizeof(s->reg_confirm));
                 RC_CYAN(s); sess_tx("\r\nCONFIRM  ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 4:  memset(s->reg_email,     0, sizeof(s->reg_email));
                 RC_CYAN(s); sess_tx("\r\nEMAIL    ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 5:  memset(s->reg_firstname, 0, sizeof(s->reg_firstname));
                 RC_CYAN(s); sess_tx("\r\nFIRST    ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 6:  memset(s->reg_lastname,  0, sizeof(s->reg_lastname));
                 RC_CYAN(s); sess_tx("\r\nLAST     ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 7:  memset(s->reg_location,  0, sizeof(s->reg_location));
                 RC_CYAN(s); sess_tx("\r\nLOCATION ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 8:
                 RC_CYAN(s); sess_tx("\r\nGRAPHICS :\r\n  ");
                 RC_LTBLUE(s); sess_tx("1");
                 RC_CYAN(s);  sess_tx("=PETSCII  ");
                 RC_LTBLUE(s); sess_tx("2");
                 RC_CYAN(s);  sess_tx("=ANSI\r\n  ");
                 RC_LTBLUE(s); sess_tx("3");
                 RC_CYAN(s);  sess_tx("=ASCII\r\nCHOICE   ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 9:
                 RC_CYAN(s); sess_tx("\r\nCOLUMNS  :\r\n  ");
                 RC_LTBLUE(s); sess_tx("1");
                 RC_CYAN(s);  sess_tx("=40  ");
                 RC_LTBLUE(s); sess_tx("2");
                 RC_CYAN(s);  sess_tx("=80  (RETURN=DEFAULT)\r\nCHOICE   ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 10:
                 RC_CYAN(s); sess_tx("\r\nROWS     :\r\n  ");
                 RC_LTBLUE(s); sess_tx("1");
                 RC_CYAN(s);  sess_tx("=24  ");
                 RC_LTBLUE(s); sess_tx("2");
                 RC_CYAN(s);  sess_tx("=25  (RETURN=DEFAULT)\r\nCHOICE   ");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 11: session_clear_screen(s);
                 sess_reg_show_confirm(s);
                 RC_CYAN(s); sess_tx("\r\nENTER # TO CHANGE, RETURN=OK");
                 RC_WHITE(s); sess_tx(": ");                        break;
        case 13:
                 session_clear_screen(s);
                 RC_CYAN(s); sess_tx("\r\n");
                 RC_RVON(s);
                 sess_tx("IT'S A NEW USER");
                 RC_RVOFF(s);
                 RC_CYAN(s);
                 sess_tx("\r\n\r\nWELCOME TO ");
                 RC_WHITE(s); session_emit(s, bbs_cfg.bbs_name);  /* mixed-case: must translate */
                 RC_CYAN(s); sess_tx(", ");
                 RC_WHITE(s); sess_tx(s->handle);
                 RC_CYAN(s); sess_tx("!\r\n\r\nYOU ARE USER ");
                 RC_WHITE(s);
                 {
                     char uid_str[6];
                     sprintf(uid_str, "%u", (unsigned)s->user_id);
                     sess_tx(uid_str);
                 }
                 RC_CYAN(s); sess_tx(".\r\n\r\nYOUR SYSOP IS ");
                 RC_WHITE(s); session_emit(s, bbs_cfg.sysop_name);  /* mixed-case: must translate */
                 RC_CYAN(s); sess_tx(".\r\n\r\n");
                 sess_tx("ENJOY.\r\n\r\n");
                 sess_tx("[PRESS RETURN TO CONTINUE]");
                 RC_WHITE(s);                                        break;
        default: break;
    }
}

void session_reg_step(session_t *s, u8 ch)
{
    static const u8 field_steps[9] = {1, 2, 4, 5, 6, 7, 8, 9, 10};
    bbs_err_t err;

    switch (s->reg_step) {

        case 0: /* Wait for Y/N confirmation after g.newuser */
            if (ch == 'Y' || ch == 'y') {
                sess_tx("Y");
                session_clear_screen(s);
                reg_start_step(s, 1);
            } else if (ch == 'N' || ch == 'n') {
                sess_tx("N");
                RC_CYAN(s); sess_tx("\r\nOK, COME BACK WHEN YOU'RE READY!\r\n");
                s->state = SESS_LOGOFF;
            }
            break;

        case 1: /* Handle */
            if (sess_reg_collect(s, ch, s->handle, 15, 0)) {
                if (strlen(s->handle) < 2) {
                    RC_CYAN(s); sess_tx("\r\nTOO SHORT. MIN 2 CHARS.\r\nHANDLE");
                    RC_WHITE(s); sess_tx(": ");
                } else {
                    err = auth_validate_handle(s->handle, bbs_cfg.device_system);
                    if (err == BBS_EEXIST) {
                        RC_CYAN(s); sess_tx("\r\nHANDLE TAKEN OR RESERVED.\r\nHANDLE");
                        RC_WHITE(s); sess_tx(": ");
                        memset(s->handle, 0, sizeof(s->handle));
                    } else if (err != BBS_OK) {
                        RC_CYAN(s); sess_tx("\r\nINVALID HANDLE.\r\nHANDLE");
                        RC_WHITE(s); sess_tx(": ");
                        memset(s->handle, 0, sizeof(s->handle));
                    } else if (s->reg_editing) {
                        s->reg_editing = 0;
                        reg_start_step(s, 11);
                    } else {
                        reg_start_step(s, 2);
                    }
                }
            }
            break;

        case 2: /* Password */
            if (sess_reg_collect(s, ch, s->password, USER_PASSWORD_MAX, '*')) {
                if (strlen(s->password) < USER_PASSWORD_MIN) {
                    RC_CYAN(s); sess_tx("\r\nTOO SHORT. MIN 4 CHARS.\r\nPASSWORD");
                    RC_WHITE(s); sess_tx(": ");
                    memset(s->password, 0, sizeof(s->password));
                } else {
                    reg_start_step(s, 3);
                }
            }
            break;

        case 3: /* Confirm password */
            if (sess_reg_collect(s, ch, s->reg_confirm, USER_PASSWORD_MAX, '*')) {
                if (strcmp(s->password, s->reg_confirm) != 0) {
                    RC_CYAN(s); sess_tx("\r\nMISMATCH. TRY AGAIN.\r\nPASSWORD");
                    RC_WHITE(s); sess_tx(": ");
                    memset(s->password,    0, sizeof(s->password));
                    memset(s->reg_confirm, 0, sizeof(s->reg_confirm));
                    s->reg_editing = 0;
                    s->reg_step = 2;
                } else if (s->reg_editing) {
                    s->reg_editing = 0;
                    reg_start_step(s, 11);
                } else {
                    reg_start_step(s, 4);
                }
            }
            break;

        case 4: /* Email */
            if (sess_reg_collect(s, ch, s->reg_email, 31, 0)) {
                if (s->reg_editing) { s->reg_editing = 0; reg_start_step(s, 11); }
                else                { reg_start_step(s, 5); }
            }
            break;

        case 5: /* First name */
            if (sess_reg_collect(s, ch, s->reg_firstname, 15, 0)) {
                if (s->reg_editing) { s->reg_editing = 0; reg_start_step(s, 11); }
                else                { reg_start_step(s, 6); }
            }
            break;

        case 6: /* Last name */
            if (sess_reg_collect(s, ch, s->reg_lastname, 15, 0)) {
                if (s->reg_editing) { s->reg_editing = 0; reg_start_step(s, 11); }
                else                { reg_start_step(s, 7); }
            }
            break;

        case 7: /* Location */
            if (sess_reg_collect(s, ch, s->reg_location, 20, 0)) {
                if (s->reg_editing) { s->reg_editing = 0; reg_start_step(s, 11); }
                else                { reg_start_step(s, 8); }
            }
            break;

        case 8: /* Graphics mode — single keypress */
            if      (ch == '1') { s->term_mode = TERM_PETSCII;     sess_tx(" PETSCII\r\n");   }
            else if (ch == '2') { s->term_mode = TERM_ANSI_CP437;  sess_tx(" ANSI/CP437\r\n"); }
            else if (ch == '3') { s->term_mode = TERM_ASCII;       sess_tx(" ASCII\r\n");      }
            else break;
            if (s->reg_editing) { s->reg_editing = 0; reg_start_step(s, 11); }
            else                { reg_start_step(s, 9); }
            break;

        case 9: /* Columns */
            if      (ch == '\r' || ch == '\n') { /* keep default */ }
            else if (ch == '1') { s->term_width = 40; sess_tx("40"); }
            else if (ch == '2') { s->term_width = 80; sess_tx("80"); }
            else break;
            sess_tx("\r\n");
            if (s->reg_editing) { s->reg_editing = 0; reg_start_step(s, 11); }
            else                { reg_start_step(s, 10); }
            break;

        case 10: /* Rows */
            if      (ch == '\r' || ch == '\n') { /* keep default */ }
            else if (ch == '1') { s->term_rows = 24; sess_tx("24"); }
            else if (ch == '2') { s->term_rows = 25; sess_tx("25"); }
            else break;
            sess_tx("\r\n");
            reg_start_step(s, 11);
            break;

        case 11: /* Confirmation + finalize */
            if (ch == '\r' || ch == '\n') {
                err = auth_register_new(s);
                if (err == BBS_OK) {
                    char id_str[4];
                    RC_RVON(s); RC_CYAN(s);
                    sess_tx("\r\n\r\nACCOUNT CREATED!\r\n");
                    RC_RVOFF(s);
                    RC_CYAN(s); sess_tx("ID");
                    RC_WHITE(s); sess_tx(":       ");
                    sprintf(id_str, "%u", (unsigned)s->user_id);
                    sess_tx(id_str); sess_tx("\r\n");
                    RC_CYAN(s); sess_tx("HANDLE");
                    RC_WHITE(s); sess_tx(":   "); sess_tx(s->handle); sess_tx("\r\n");
                    RC_CYAN(s); sess_tx("PASSWORD");
                    RC_WHITE(s); sess_tx(": "); sess_tx(s->password);
                    RC_YELLOW(s);
                    sess_tx("\r\n\r\nWRITE THESE DOWN! PRESS RETURN...");
                    RC_WHITE(s);
                    s->reg_step = 12;
                } else if (err == BBS_EEXIST) {
                    RC_CYAN(s); sess_tx("\r\nHANDLE TAKEN. CHOOSE ANOTHER.\r\n");
                    reg_start_step(s, 1);
                } else if (err == BBS_EFULL) {
                    RC_CYAN(s); sess_tx("\r\nUSER DATABASE FULL. GOODBYE.\r\n");
                    s->state = SESS_LOGOFF;
                } else {
                    RC_CYAN(s); sess_tx("\r\nREGISTRATION FAILED. GOODBYE.\r\n");
                    s->state = SESS_LOGOFF;
                }
            } else if (ch >= '1' && ch <= '9') {
                u8 idx = (u8)(ch - '1');
                s->reg_editing = 1;
                reg_start_step(s, field_steps[idx]);
            }
            break;

        case 12: /* Wait for RETURN after credential display */
            if (ch == '\r' || ch == '\n') {
                reg_start_step(s, 13);
            }
            break;

        case 13: /* Wait for RETURN after welcome screen */
            if (ch == '\r' || ch == '\n') {
                s->state = SESS_IN_MENU;
            }
            break;
    }
}

#pragma code(code)
#pragma data(data)
#pragma bss(bss)
