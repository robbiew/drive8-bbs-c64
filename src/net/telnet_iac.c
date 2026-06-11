/* Telnet IAC filter implementation. Ported from drive8-bbs. */
#include "telnet_iac.h"
#include <string.h>

#define IAC          0xFF
#define DONT         0xFE
#define DO           0xFD
#define WONT         0xFC
#define WILL         0xFB
#define SB           0xFA
#define SE           0xF0
#define OPT_ECHO     0x01
#define OPT_TERMTYPE 0x18
#define SEND         0x01
#define IS           0x00

static void queue(telnet_filter_t *f, const u8 *bytes, u8 n)
{
    for (u8 i = 0; i < n && f->reply_len < TELNET_REPLY_MAX; i++)
        f->reply[f->reply_len++] = bytes[i];
}

void telnet_filter_init(telnet_filter_t *f)
{
    memset(f, 0, sizeof(*f));
    const u8 hello[] = {
        IAC, WILL, OPT_ECHO,    /* server will echo — suppress client local echo */
        IAC, DO, OPT_TERMTYPE
    };
    queue(f, hello, sizeof(hello));
    f->sent_initial = TRUE;
}

u8 telnet_filter_take_reply(telnet_filter_t *f, u8 *out, u8 max)
{
    u8 n = (f->reply_len < max) ? f->reply_len : max;
    memcpy(out, f->reply, n);
    // cppcheck-suppress knownConditionTrueFalse
    if (n < f->reply_len) memmove(f->reply, f->reply + n, f->reply_len - n);
    f->reply_len -= n;
    return n;
}

const char *telnet_filter_term(const telnet_filter_t *f)
{
    return f->term_type;
}

u8 telnet_filter_feed(telnet_filter_t *f, u8 in, u8 *out)
{
    switch (f->state) {
    case 0:
        if (in == IAC) { f->state = 1; return 0; }
        *out = in;
        return 1;
    case 1:
        switch (in) {
        case IAC: *out = IAC; f->state = 0; return 1;
        case DO:   f->state = 2; return 0;
        case DONT: f->state = 3; return 0;
        case WILL: f->state = 4; return 0;
        case WONT: f->state = 5; return 0;
        case SB:   f->state = 6; f->sb_len = 0; return 0;
        default:   f->state = 0; return 0;
        }
    case 2: {
        const u8 r[] = { IAC, WONT, in };
        queue(f, r, 3);
        f->state = 0;
        return 0;
    }
    case 3: {
        const u8 r[] = { IAC, WONT, in };
        queue(f, r, 3);
        f->state = 0;
        return 0;
    }
    case 4:
        if (in == OPT_TERMTYPE) {
            const u8 ask[] = { IAC, SB, OPT_TERMTYPE, SEND, IAC, SE };
            queue(f, ask, sizeof(ask));
        } else {
            const u8 r[] = { IAC, DONT, in };
            queue(f, r, 3);
        }
        f->state = 0;
        return 0;
    case 5: {
        const u8 r[] = { IAC, DONT, in };
        queue(f, r, 3);
        f->state = 0;
        return 0;
    }
    case 6:
        if (in == IAC) { f->state = 7; return 0; }
        if (f->sb_len == 0) f->sb_opt = in;
        else if (f->sb_len <= TELNET_TERM_MAX) f->sb_buf[f->sb_len - 1] = in;
        f->sb_len++;
        return 0;
    case 7:
        if (in == SE) {
            if (f->sb_opt == OPT_TERMTYPE && f->sb_len >= 2 &&
                f->sb_buf[0] == IS) {
                u8 vlen = f->sb_len - 2;
                if (vlen > TELNET_TERM_MAX) vlen = TELNET_TERM_MAX;
                memcpy(f->term_type, f->sb_buf + 1, vlen);
                f->term_type[vlen] = '\0';
            }
            f->state = 0;
        } else if (in == IAC) {
            if (f->sb_len > 0 && f->sb_len <= TELNET_TERM_MAX) f->sb_buf[f->sb_len - 1] = IAC;
            f->sb_len++;
            f->state = 6;
        } else {
            f->state = 0;
        }
        return 0;
    }
    f->state = 0;
    return 0;
}
