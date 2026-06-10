/* ANSI streaming filter — converts CP437 + ANSI escape sequences to
 * PETSCII when needed, or passes through for ANSI_CP437 mode.
 *
 * Ported from drive8-bbs src/term/ansi.c.
 */
#include "term_internal.h"

/* PETSCII colour codes for SGR 30-37 (FG). */
static const u8 sgr_fg_to_petscii[8] = {
    0x90, /* 30 black */
    0x1C, /* 31 red */
    0x1E, /* 32 green */
    0x9F, /* 33 yellow (approx) */
    0x1F, /* 34 blue */
    0x81, /* 35 magenta */
    0x95, /* 36 cyan */
    0x05  /* 37 white */
};

void term_filter_init(term_filter_t *f, term_mode_t mode)
{
    f->mode  = (u8)mode;
    f->state = 0;
    f->len   = 0;
}

static u8 emit_byte(const term_filter_t *f, u8 b, u8 *out, u8 max)
{
    switch ((term_mode_t)f->mode) {
    case TERM_PETSCII:    return term_xlate_byte_petscii(b, out, max);
    case TERM_ANSI_CP437: if (max < 1) return 0; out[0] = b; return 1;
    case TERM_ASCII:      return term_xlate_byte_ascii(b, out, max);
    }
    return 0;
}

static u8 handle_csi(term_filter_t *f, u8 final_byte, u8 *out, u8 max)
{
    if ((term_mode_t)f->mode == TERM_ANSI_CP437) {
        /* Pass through: rebuild ESC [ ... <final>. */
        if (max < (u8)(f->len + 3)) return 0;
        u8 n = 0;
        out[n++] = 0x1B;
        out[n++] = '[';
        for (u8 i = 0; i < f->len; i++) out[n++] = f->buf[i];
        out[n++] = final_byte;
        return n;
    }
    if ((term_mode_t)f->mode == TERM_PETSCII && final_byte == 'm') {
        int v = 0;
        for (u8 i = 0; i < f->len; i++) {
            if (f->buf[i] >= '0' && f->buf[i] <= '9')
                v = v * 10 + (f->buf[i] - '0');
            else
                break;
        }
        if (v == 0) {
            if (max < 1) return 0;
            out[0] = 0x05; return 1;   /* PETSCII white = reset */
        }
        if (v >= 30 && v <= 37) {
            if (max < 1) return 0;
            out[0] = sgr_fg_to_petscii[v - 30];
            return 1;
        }
    }
    /* ASCII or unhandled CSI: drop. */
    return 0;
}

u8 term_filter_feed(term_filter_t *f, u8 in, u8 *out, u8 max)
{
    switch (f->state) {
    case 0:
        if (in == 0x1B) { f->state = 1; return 0; }
        return emit_byte(f, in, out, max);
    case 1:   /* saw ESC */
        if (in == '[') { f->state = 2; f->len = 0; return 0; }
        f->state = 0;
        return emit_byte(f, in, out, max);
    case 2:   /* in CSI */
        if ((in >= '0' && in <= '9') || in == ';') {
            if (f->len < sizeof(f->buf)) f->buf[f->len++] = in;
            return 0;
        }
        f->state = 0;
        return handle_csi(f, in, out, max);
    }
    f->state = 0;
    return 0;
}
