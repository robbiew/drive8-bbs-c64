/* Host unit tests for the telnet IAC filter state machine. */
#include "host.h"
#include "telnet_iac.h"

static u8 feed_all(telnet_filter_t *f, const u8 *in, u8 n, u8 *app) {
    u8 count = 0, out;
    u8 i;
    for (i = 0; i < n; i++)
        if (telnet_filter_feed(f, in[i], &out)) app[count++] = out;
    return count;
}

int main(void) {
    telnet_filter_t f;
    u8 app[64], reply[32], n;

    /* init queues IAC WILL ECHO + IAC DO TERMTYPE */
    telnet_filter_init(&f);
    n = telnet_filter_take_reply(&f, reply, sizeof(reply));
    {
        static const u8 want[] = {0xFF,0xFB,0x01, 0xFF,0xFD,0x18};
        EXPECT_EQ("init.reply_len", n, 6);
        EXPECT_MEM("init.reply", reply, want, 6);
    }

    /* plain bytes pass through untouched */
    n = feed_all(&f, (const u8 *)"HI", 2, app);
    EXPECT_EQ("plain.count", n, 2);
    EXPECT_MEM("plain.bytes", app, "HI", 2);

    /* IAC IAC escapes to one literal 0xFF data byte */
    {
        static const u8 in[] = {0xFF,0xFF};
        n = feed_all(&f, in, 2, app);
        EXPECT_EQ("iaciac.count", n, 1);
        EXPECT_EQ("iaciac.byte", app[0], 0xFF);
    }

    /* DO <opt> is answered IAC WONT <opt> and not passed to the app */
    {
        static const u8 in[] = {0xFF,0xFD,0x22};
        static const u8 want[] = {0xFF,0xFC,0x22};
        n = feed_all(&f, in, 3, app);
        EXPECT_EQ("do.app", n, 0);
        n = telnet_filter_take_reply(&f, reply, sizeof(reply));
        EXPECT_EQ("do.reply_len", n, 3);
        EXPECT_MEM("do.reply", reply, want, 3);
    }

    /* WILL TERMTYPE triggers the SB TERMTYPE SEND request */
    {
        static const u8 in[] = {0xFF,0xFB,0x18};
        static const u8 want[] = {0xFF,0xFA,0x18,0x01,0xFF,0xF0};
        feed_all(&f, in, 3, app);
        n = telnet_filter_take_reply(&f, reply, sizeof(reply));
        EXPECT_EQ("willtt.reply_len", n, 6);
        EXPECT_MEM("willtt.reply", reply, want, 6);
    }

    /* SB TERMTYPE IS "ANSI" IAC SE captures the terminal type */
    {
        static const u8 in[] = {0xFF,0xFA,0x18,0x00,'A','N','S','I',0xFF,0xF0};
        n = feed_all(&f, in, sizeof(in), app);
        EXPECT_EQ("sb.app", n, 0);
        EXPECT_STR("sb.term", telnet_filter_term(&f), "ANSI");
    }

    /* over-long terminal type is truncated, never overflows sb_buf[8] */
    {
        static const u8 in[] = {0xFF,0xFA,0x18,0x00,
            'X','T','E','R','M','2','5','6','C','O','L','O','R',0xFF,0xF0};
        feed_all(&f, in, sizeof(in), app);
        EXPECT_EQ("sblong.bounded",
                  (long)(strlen(telnet_filter_term(&f)) <= TELNET_TERM_MAX), 1);
    }

    return test_summary("telnet_iac");
}
