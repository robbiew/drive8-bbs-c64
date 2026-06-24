#ifndef NET_ZMODEM_H
#define NET_ZMODEM_H

#include "bbs/types.h"
#include "bbs/session.h"

typedef enum {
    ZMODEM_OK     = 0,
    ZMODEM_ERR    = 1,
    ZMODEM_CANCEL = 2,
} zmodem_result_t;

/* Zmodem frame types */
#define ZRQINIT  0   /* request receiver to send ZRINIT */
#define ZRINIT   1   /* receiver capabilities */
#define ZSINIT   2   /* sender init */
#define ZACK     3   /* acknowledge */
#define ZFILE    4   /* file name and info */
#define ZSKIP    5   /* skip file */
#define ZNAK     6   /* last packet bad (CRC error) */
#define ZABORT   7   /* abort session */
#define ZFIN     8   /* end of session */
#define ZRPOS    9   /* resume data from this position */
#define ZDATA    10  /* data packet(s) follow */
#define ZEOF     11  /* end of file */
#define ZFERR    12  /* fatal read or write error detected */
#define ZCRC     13  /* request for file CRC */
#define ZCHALLENGE 14
#define ZCOMPL   15
#define ZCAN     16
#define ZFREECNT 17
#define ZCOMMAND 18
#define ZSTDERR  19

/* Zmodem header encoding types */
#define ZBIN     0x41  /* 'A' — binary header, CRC-16 */
#define ZHEX     0x42  /* 'B' — hex header, CRC-16 */
#define ZBIN32   0x43  /* 'C' — binary header, CRC-32 */

/* Special bytes */
#define ZDLE     0x18  /* ^X — Zmodem data-link escape */
#define ZPAD     0x2A  /* '*' — padding */
#define ZDLEE    0x58  /* 'X' — escaped ZDLE (ZDLE ^ 0x40) */

/* Data subpacket end markers (follow ZDLE in stream) */
#define ZCRCE    0x68  /* 'h' — end of file */
#define ZCRCG    0x69  /* 'i' — more data, no ACK */
#define ZCRCQ    0x6A  /* 'j' — more data, send ZACK */
#define ZCRCW    0x6B  /* 'k' — wait for ZACK */

/* ZRINIT flags (ZF0 byte) */
#define CANFDX   0x01  /* full duplex */
#define CANOVIO  0x02  /* overlap I/O and disk */

/* Send a file via Zmodem (BBS → caller).
 * __noinline prevents the interprocedural optimizer from folding these into
 * the resident xfer.c shim, which would leave zmodem_code empty. */
__noinline zmodem_result_t zmodem_send(session_t *s, u8 device, u8 drive,
                                        const char *filename);

/* Receive a file via Zmodem (caller → BBS). */
__noinline zmodem_result_t zmodem_recv(session_t *s, u8 device, u8 drive,
                                        const char *filename);

#endif /* NET_ZMODEM_H */
