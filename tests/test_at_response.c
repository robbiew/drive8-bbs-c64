/* Host unit tests for the AT modem result-code parser. */
#include "host.h"
#include "at_response.h"

static at_event_t feed_str(at_parser_t *p, const char *s) {
    at_event_t last = AT_EVT_NONE;
    u8 ob, oa;
    for (; *s; s++) {
        at_event_t e = at_parser_feed(p, (u8)*s, &ob, &oa);
        if (e != AT_EVT_NONE) last = e;
    }
    return last;
}

int main(void) {
    at_parser_t p;
    u8 ob, oa;

    at_parser_init(&p);
    EXPECT_EQ("ring",    feed_str(&p, "RING\r"), AT_EVT_RING);
    EXPECT_EQ("ok",      feed_str(&p, "OK\r"), AT_EVT_OK);
    EXPECT_EQ("error",   feed_str(&p, "ERROR\r"), AT_EVT_ERROR);
    EXPECT_EQ("connect", feed_str(&p, "CONNECT 9600\r"), AT_EVT_CONNECT);
    EXPECT_EQ("connected_flag", p.connected, 1);

    /* post-CONNECT bytes pass through as application data */
    at_parser_feed(&p, 'H', &ob, &oa);
    EXPECT_EQ("passthru.app", oa, 1);
    EXPECT_EQ("passthru.byte", ob, 'H');

    /* in-band NO CARRIER (only hangup signal under VICE/tcpser) disconnects */
    EXPECT_EQ("nocarrier", feed_str(&p, "\rNO CARRIER\r"), AT_EVT_NOCARRIER);
    EXPECT_EQ("disconnected", p.connected, 0);

    /* a 36-char garbage line cannot overflow line[15] */
    at_parser_init(&p);
    EXPECT_EQ("garbage",
              feed_str(&p, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r"),
              AT_EVT_NONE);

    return test_summary("at_response");
}
