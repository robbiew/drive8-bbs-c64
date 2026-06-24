/* punter.c — Punter Single file transfer protocol.  OVL_FILES section.
 *
 * Protocol references:
 *   CGTerm  punter.c (receiver):  github.com/MagerValp/CGTerm
 *   ccgmsterm punter.c (sender):  github.com/mist64/ccgmsterm
 *
 * Block format (7-byte header + 0-248 bytes payload, max 255 bytes total):
 *   buf[0-1] u16 LE  additive checksum (sum of buf[4+])
 *   buf[2-3] u16 LE  cyclic checksum   (XOR + 16-bit left-rotate of buf[4+])
 *   buf[4]   u8      NEXT block total size (7 + next payload length)
 *   buf[5-6] u16 LE  block index; 0xFFFF = final block
 *   buf[7+]  u8[]    payload (0-248 bytes)
 *
 * Handshake tokens (3 bytes each): GOO ACK S/B SYN BAD
 *
 * Timing uses the KERNAL jiffy low byte at $00A2 (~50 Hz PAL).
 */
#include "bbs/overlay.h"
#include "bbs/session.h"
#include "bbs/net.h"
#include "bbs/hal/disk.h"
#include "bbs/cfg.h"
#include "net/punter.h"
#include <string.h>
#include <stdio.h>

#pragma code(files_code)
#pragma data(files_data)
#pragma bss(files_bss)

/* ── Constants ─────────────────────────────────────────────────────────── */
#define BLK_MAX      255u
#define PAYLOAD_MAX  248u
#define P_RETRIES     10

/* Jiffy timeouts (~50 Hz PAL; NTSC fires faster so these are conservative) */
#define J_TOK   50    /* ~1 s per 3-byte token */

/* ── Block buffer and read-ahead (BSS: zero at load) ───────────────────── */
static u8 s_blk[BLK_MAX];
static u8 s_nxt[PAYLOAD_MAX];  /* read-ahead for send path */
static u8 s_nxt_len;

/* ── Token constants ────────────────────────────────────────────────────── */
static const u8 T_GOO[3] = {'G','O','O'};
static const u8 T_ACK[3] = {'A','C','K'};
static const u8 T_SB[3]  = {'S','/','B'};
static const u8 T_SYN[3] = {'S','Y','N'};
static const u8 T_BAD[3] = {'B','A','D'};

/* ── Timing ─────────────────────────────────────────────────────────────── */
static u8 j_now(void)              { return *(volatile u8 *)0x00A2; }
static bool_t j_over(u8 s, u8 lim) { return (u8)(j_now() - s) >= lim; }

/* ── I/O primitives ─────────────────────────────────────────────────────── */

static void ptx(const u8 *buf, u8 n)
{
    u16 sent;
    net_tx_raw(buf, n, &sent);
}

/* Receive `n` bytes with jiffy timeout anchored at `jstart`.
 * Returns TRUE if all bytes received before j_over(jstart, jlim). */
static bool_t prx(u8 *buf, u8 n, u8 jstart, u8 jlim)
{
    u8 i = 0;
    while (i < n) {
        u16 got;
        u8 b;
        if (net_rx_raw(&b, 1, &got) == BBS_OK && got == 1) { buf[i++] = b; continue; }
        if (j_over(jstart, jlim)) return FALSE;
    }
    return TRUE;
}

static bool_t teq(const u8 *a, const u8 *b)
{ return a[0]==b[0] && a[1]==b[1] && a[2]==b[2]; }

/* Send tx_tok (if non-NULL) then receive and match rx_tok.
 * Retries P_RETRIES times on timeout or wrong token.
 * Passing tx_tok=NULL does receive-only with retries. */
static bool_t p_hs(const u8 *tx_tok, const u8 *rx_tok)
{
    u8 buf[3], tries = P_RETRIES;
    while (tries--) {
        u8 jstart;
        if (tx_tok) ptx(tx_tok, 3);
        jstart = j_now();
        if (prx(buf, 3, jstart, J_TOK) && teq(buf, rx_tok)) return TRUE;
    }
    return FALSE;
}

/* ── Checksums ──────────────────────────────────────────────────────────── */

static void p_csum_write(u8 len)
{
    u16 add = 0, clc = 0;
    u8 i;
    for (i = 4; i < len; i++) {
        u8 b = s_blk[i];
        add += b;
        clc ^= b;
        clc = (u16)((clc << 1) | (clc >> 15));
    }
    s_blk[0] = (u8)add;        s_blk[1] = (u8)(add >> 8);
    s_blk[2] = (u8)clc;        s_blk[3] = (u8)(clc >> 8);
}

static bool_t p_csum_ok(u8 len)
{
    u16 add = 0, clc = 0;
    u8 i;
    for (i = 4; i < len; i++) {
        u8 b = s_blk[i];
        add += b;
        clc ^= b;
        clc = (u16)((clc << 1) | (clc >> 15));
    }
    return (((u16)s_blk[0] | ((u16)s_blk[1] << 8)) == add) &&
           (((u16)s_blk[2] | ((u16)s_blk[3] << 8)) == clc);
}

/* ── Send side (BBS → terminal, download) ───────────────────────────────── */

/* Send s_blk[0..len-1]; retry on BAD; return TRUE on GOO. */
static bool_t p_send_blk(u8 len)
{
    u8 buf[3], tries = P_RETRIES;
    while (tries--) {
        u8 jstart;
        ptx(s_blk, len);
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK)) continue;
        if (teq(buf, T_GOO)) return TRUE;
        if (teq(buf, T_BAD)) continue;   /* remote requests resend */
    }
    return FALSE;
}

punter_result_t punter_send(session_t *s, u8 device, u8 drive,
                             const char *filename, u8 filetype)
{
    bbs_err_t err;
    u16 index;
    u8 cur_len;
    bool_t cur_is_last, next_eof;
    i16 n;

    (void)s;   /* carrier checked implicitly by net_rx returning EAGAIN */

    err = disk_open(device, drive, filename, DISK_READ);
    if (err != BBS_OK) return PUNTER_ERR;

    /* ── Phase A: file type ──────────────────────────────────────────── */
    /* A1: both sides announce readiness */
    if (!p_hs(T_GOO, T_GOO)) goto fail;

    /* A2: send 8-byte filetype block (7-hdr + 1-payload) */
    if (!p_hs(T_ACK, T_SB)) goto fail;
    s_blk[4] = 7;           /* next block = info block (7 bytes) */
    s_blk[5] = 0xFF; s_blk[6] = 0xFF;   /* index 0xFFFF */
    s_blk[7] = filetype;
    p_csum_write(8);
    if (!p_send_blk(8)) goto fail;

    /* A3: end-of-phase-A sync */
    if (!p_hs(T_ACK, T_SB))   goto fail;
    if (!p_hs(T_SYN, T_SYN))  goto fail;
    ptx(T_SB, 3);

    /* ── Phase B: file data ──────────────────────────────────────────── */
    /* B1: wait for terminal ready */
    if (!p_hs(NULL, T_GOO)) goto fail;

    /* Pre-read first chunk so we know its size for the info block's buf[4] */
    n = disk_read(s_nxt, PAYLOAD_MAX);
    s_nxt_len = (n > 0) ? (u8)n : 0;
    next_eof = disk_eof();
    cur_is_last = (s_nxt_len < PAYLOAD_MAX) || next_eof;

    /* Info block: 7 bytes, no payload, buf[4] = first data block total size */
    if (!p_hs(T_ACK, T_SB)) goto fail;
    s_blk[4] = (u8)(s_nxt_len + 7);
    s_blk[5] = 0; s_blk[6] = 0;    /* index 0 */
    p_csum_write(7);
    if (!p_send_blk(7)) goto fail;

    /* Data blocks (index starts at 1; last block gets index 0xFFFF) */
    index = 1;
    for (;;) {
        u8 blk_len;

        cur_len = s_nxt_len;
        memcpy(s_blk + 7, s_nxt, cur_len);

        /* Read ahead to know next block size (needed for buf[4] of current) */
        if (!cur_is_last) {
            n = disk_read(s_nxt, PAYLOAD_MAX);
            s_nxt_len = (n > 0) ? (u8)n : 0;
            next_eof  = disk_eof();
            cur_is_last = (s_nxt_len < PAYLOAD_MAX) || next_eof;
            s_blk[4] = (u8)(s_nxt_len + 7);
        } else {
            s_blk[4] = 7;   /* no more data follows */
        }

        if (cur_is_last) {
            s_blk[5] = 0xFF; s_blk[6] = 0xFF;   /* index 0xFFFF = last */
        } else {
            s_blk[5] = (u8)(index & 0xFF);
            s_blk[6] = (u8)(index >> 8);
        }

        blk_len = (u8)(cur_len + 7);
        p_csum_write(blk_len);
        if (!p_hs(T_ACK, T_SB))     goto fail;
        if (!p_send_blk(blk_len))   goto fail;

        if (cur_is_last) break;
        index++;
    }

    /* B3: end-of-phase-B sync */
    if (!p_hs(T_ACK, T_SB))  goto fail;
    if (!p_hs(T_SYN, T_SYN)) goto fail;
    ptx(T_SB, 3);

    disk_close();
    return PUNTER_OK;

fail:
    disk_close();
    return PUNTER_ERR;
}

/* ── Receive side (terminal → BBS, upload) ──────────────────────────────── */

/* Receive one data block for the upload path.
 * The terminal sends: ACK → [waits for S/B] → block bytes.
 * We send S/B to trigger, receive `len` bytes, verify, reply GOO/BAD. */
static bool_t p_recv_blk(u8 len)
{
    u8 buf[3], tries = P_RETRIES;
    while (tries--) {
        u8 jstart;
        /* Wait for ACK from terminal sender */
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK)) return FALSE;
        if (!teq(buf, T_ACK)) continue;

        /* Prompt terminal to send the block */
        ptx(T_SB, 3);

        /* Receive block bytes */
        jstart = j_now();
        if (!prx(s_blk, len, jstart, J_TOK)) {
            ptx(T_BAD, 3);   /* timeout: request retry */
            continue;
        }
        if (p_csum_ok(len)) {
            ptx(T_GOO, 3);
            return TRUE;
        }
        ptx(T_BAD, 3);   /* bad checksum: request retry */
    }
    return FALSE;
}

punter_result_t punter_recv(session_t *s, u8 device, u8 drive,
                             const char *filename, u8 *out_filetype)
{
    bbs_err_t err;
    u8 next_blk_len;
    u16 blk_index;

    (void)s;

    if (out_filetype) *out_filetype = 1;   /* default SEQ */

    err = disk_open(device, drive, filename, DISK_WRITE);
    if (err != BBS_OK) return PUNTER_ERR;

    /* ── Phase A: file type ──────────────────────────────────────────── */
    /* A1 */
    if (!p_hs(T_GOO, T_GOO)) goto fail;

    /* A2: receive 8-byte filetype block */
    {
        u8 buf[3], jstart;
        /* Wait for ACK from terminal sender */
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK) || !teq(buf, T_ACK)) goto fail;
        ptx(T_SB, 3);
        jstart = j_now();
        if (!prx(s_blk, 8, jstart, J_TOK)) { ptx(T_BAD, 3); goto fail; }
        if (!p_csum_ok(8))                  { ptx(T_BAD, 3); goto fail; }
        if (out_filetype) *out_filetype = s_blk[7];
        ptx(T_GOO, 3);
    }

    /* A3 */
    {
        u8 buf[3], jstart;
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK) || !teq(buf, T_ACK)) goto fail;
        ptx(T_SB, 3);
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK) || !teq(buf, T_SYN)) goto fail;
        ptx(T_SYN, 3);
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK) || !teq(buf, T_SB))  goto fail;
    }

    /* ── Phase B: file data ──────────────────────────────────────────── */
    /* B1: announce we are ready for data */
    ptx(T_GOO, 3);

    /* Info block: always 7 bytes */
    if (!p_recv_blk(7)) goto fail;
    next_blk_len = s_blk[4];
    blk_index    = (u16)s_blk[5] | ((u16)s_blk[6] << 8);

    /* Data blocks */
    while (blk_index < 0xFF00u && next_blk_len >= 7) {
        u8 payload_len;
        if (!p_recv_blk(next_blk_len)) goto fail;
        payload_len  = (u8)(next_blk_len - 7);
        /* Write payload to disk */
        if (payload_len > 0) {
            u8 i;
            for (i = 0; i < payload_len; i++) {
                if (disk_putc((char)s_blk[7 + i]) != BBS_OK) goto fail;
            }
        }
        next_blk_len = s_blk[4];
        blk_index    = (u16)s_blk[5] | ((u16)s_blk[6] << 8);
    }

    /* B3: end-of-phase-B sync */
    {
        u8 buf[3], jstart;
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK) || !teq(buf, T_ACK)) goto fail;
        ptx(T_SB, 3);
        jstart = j_now();
        if (!prx(buf, 3, jstart, J_TOK) || !teq(buf, T_SYN)) goto fail;
        ptx(T_SYN, 3);
        jstart = j_now();
        (void)prx(buf, 3, jstart, J_TOK);   /* consume final S/B */
    }

    disk_close();
    return PUNTER_OK;

fail:
    disk_close();
    return PUNTER_ERR;
}
