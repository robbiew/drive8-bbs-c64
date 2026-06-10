/* CBM IEC sequential disk I/O using oscar64 kernalio. */
#include "bbs/hal/disk.h"
#include "bbs/config.h"
#include <c64/kernalio.h>
#include <string.h>

char disk_errmsg[40] = { 0 };

/* Scratch channel 15 and re-open for status read. */
static u8 read_status(u8 device)
{
    int r;
    krnio_close(CFG_FNUM_CMD);
    krnio_setnam("");
    krnio_open(CFG_FNUM_CMD, device, 15);
    r = krnio_gets(CFG_FNUM_CMD, disk_errmsg, (int)sizeof(disk_errmsg));
    krnio_close(CFG_FNUM_CMD);
    if (r <= 0) return 99;
    return (u8)((disk_errmsg[0] - '0') * 10 + (disk_errmsg[1] - '0'));
}

static bbs_err_t check_status(u8 device)
{
    u8 code = read_status(device);
    if (code < 20)  return BBS_OK;
    if (code == 62) return BBS_ENOTFOUND;
    return BBS_EIO;
}

static u8 s_open_device = 0;

bbs_err_t disk_open(u8 device, u8 drive, const char *name, disk_mode_t mode)
{
    char fname[48];
    const char *suffix;
    switch (mode) {
    case DISK_READ:   suffix = ",S,R"; break;
    case DISK_WRITE:  sprintf(fname, "@%d:%s,S,W", drive, name);
                      goto do_open;
    case DISK_APPEND: suffix = ",S,A"; break;
    default:          suffix = ",S,W"; break;
    }
    sprintf(fname, "%d:%s%s", drive, name, suffix);

do_open:
    krnio_setnam(fname);
    if (!krnio_open(CFG_FNUM_DATA, device, 2)) {
        return BBS_EIO;
    }

    /* Reset per-file EOF state: krnio_pstatus[fnum] persists across close/open.
     * A prior read to EOF leaves KRNIO_EOF, causing the next krnio_gets to
     * return 0 immediately without touching the IEC bus. */
    krnio_pstatus[CFG_FNUM_DATA] = KRNIO_OK;
    s_open_device = device;
    return BBS_OK;
}

void disk_close(void)
{
    krnio_clrchn();
    krnio_close(CFG_FNUM_DATA);
    krnio_pstatus[CFG_FNUM_DATA] = KRNIO_OK; /* reset for next open */
    s_open_device = 0;
}

i16 disk_getc(void)
{
    int v = krnio_getch(CFG_FNUM_DATA);
    if (v < 0) return -1;
    if (krnio_pstatus[CFG_FNUM_DATA] & KRNIO_EOF) return -1;
    return (i16)(v & 0xFF);
}

i16 disk_read(u8 *buf, u8 len)
{
    /* krnio_read() does ONE krnio_chkin() + N×krnio_chrin() + ONE krnio_clrchn().
     * Dramatically faster than disk_getc() for bulk sequential reads:
     * krnio_getch() negotiates the IEC bus (CHKIN+CLRCHN) for every byte. */
    int r = krnio_read(CFG_FNUM_DATA, (char *)buf, (int)len);
    if (r < 0) return -1;
    return (i16)r;
}

i16 disk_gets(char *buf, u8 len)
{
    int r = krnio_gets(CFG_FNUM_DATA, buf, (int)len);
    if (r < 0) return -1;
    return (i16)r;
}

bbs_err_t disk_putc(char c)
{
    int r = krnio_putch(CFG_FNUM_DATA, c);
    return (r >= 0) ? BBS_OK : BBS_EIO;
}

bbs_err_t disk_puts(const char *s)
{
    int r = krnio_puts(CFG_FNUM_DATA, s);
    return (r >= 0) ? BBS_OK : BBS_EIO;
}

bbs_err_t disk_putline(const char *s)
{
    bbs_err_t e = disk_puts(s);
    if (e != BBS_OK) return e;
    return disk_putc('\r');
}

bool_t disk_eof(void)
{
    return (krnio_pstatus[CFG_FNUM_DATA] & KRNIO_EOF) ? TRUE : FALSE;
}

bbs_err_t disk_scratch(u8 device, u8 drive, const char *name)
{
    char cmd[40];
    if (drive == 0) {
        sprintf(cmd, "S:%s", name);
    } else {
        sprintf(cmd, "S%d:%s", drive, name);
    }
    return disk_cmd(device, cmd);
}

bbs_err_t disk_rename(u8 device, u8 drive,
                      const char *old_name, const char *new_name)
{
    char cmd[48];
    if (drive == 0) {
        sprintf(cmd, "R:%s=%s", new_name, old_name);
    } else {
        sprintf(cmd, "R%d:%s=%s", drive, new_name, old_name);
    }
    return disk_cmd(device, cmd);
}

bbs_err_t disk_cmd(u8 device, const char *cmd)
{
    krnio_setnam(cmd);
    krnio_open(CFG_FNUM_CMD, device, 15);
    krnio_close(CFG_FNUM_CMD);
    return check_status(device);
}

u8 disk_status(u8 device)
{
    return read_status(device);
}

void disk_name_bull(char *buf, u8 board, u16 post)
{
    sprintf(buf, "B.%d.%d", (int)board, (int)post);
}

void disk_name_mail(char *buf, u16 msg_id, u8 user_id)
{
    sprintf(buf, "E.%d.%d", (int)msg_id, (int)user_id);
}

/**
 * disk_build_term_filename()
 *
 * Phase B: Build terminal-aware filename candidates.
 * Generates 4 candidates in priority order:
 *   1. G.<NAME> <mode> <width>  (most specific)
 *   2. G.<NAME> <mode>          (mode only)
 *   3. G.<NAME> <width>         (width only)
 *   4. G.<NAME>                 (generic fallback)
 *
 * Names are uppercase because c1541 stores filenames as uppercase PETSCII
 * (ASCII a-z → 0x41-0x5A), and the C64 KERNAL does a byte-exact compare.
 *
 * Example: base="login", mode=1, width=80
 *   names[0] = "G.LOGIN 1 80"
 *   names[1] = "G.LOGIN 1"
 *   names[2] = "G.LOGIN 80"
 *   names[3] = "G.LOGIN"
 */
void disk_build_term_filename(term_filename_t *out,
                              char prefix,
                              const char *name,
                              u8 mode, u8 width)
{
    if (!out || !name) return;

    /* CBM DOS filenames are stored as uppercase PETSCII by c1541 (ASCII a-z
     * is converted to PETSCII A-Z, i.e. 0x41-0x5A).  Send uppercase bytes
     * from the C64 side so the directory comparison succeeds. */
    char upper[16];
    u8 j;
    char pfx = (prefix >= 'a' && prefix <= 'z') ? (char)(prefix - 32) : prefix;
    for (j = 0; j < (u8)(sizeof(upper) - 1) && name[j]; j++) {
        upper[j] = (name[j] >= 'a' && name[j] <= 'z')
                   ? (char)(name[j] - 32) : name[j];
    }
    upper[j] = '\0';

    sprintf(out->names[0], "%c.%s %d %d", pfx, upper, (int)mode, (int)width);
    sprintf(out->names[1], "%c.%s %d", pfx, upper, (int)mode);
    sprintf(out->names[2], "%c.%s %d", pfx, upper, (int)width);
    sprintf(out->names[3], "%c.%s", pfx, upper);
}

/**
 * disk_open_with_fallback()
 *
 * Phase B: Open file with fallback chain.
 * Tries each candidate filename in priority order until one opens successfully.
 * Uses terminal mode and width to generate candidates.
 *
 * Returns:
 *   BBS_OK        — file opened and ready to read/write
 *   BBS_ENOTFOUND — no candidate file found
 *   BBS_EIO       — error opening file (other than not found)
 */
bbs_err_t disk_open_with_fallback(u8 device, u8 drive,
                                  char prefix,
                                  const char *base_name,
                                  disk_mode_t mode,
                                  u8 term_mode, u8 term_width)
{
    term_filename_t names;
    u8 i;
    // cppcheck-suppress variableScope
    bbs_err_t err;

    if (!base_name) {
        return BBS_EBADARG;
    }

    /* Build candidate filenames in priority order */
    disk_build_term_filename(&names, prefix, base_name, term_mode, term_width);

    /* Try each candidate in priority order */
    for (i = 0; i < 4; i++) {
        err = disk_open(device, drive, names.names[i], mode);

        /* Success: file opened */
        if (err == BBS_OK) {
            return BBS_OK;
        }

        /* File not found: try next candidate */
        if (err == BBS_ENOTFOUND) {
            continue;
        }

        /* Other error (disk error, permission denied, etc.):
         * Return error but don't continue fallback chain
         * (might be a real disk problem, not just file missing) */
        return err;
    }

    /* All candidates exhausted */
    return BBS_ENOTFOUND;
}

/* Parse disk status line for block counts.
 * CBM DOS status format (1581/SD2IEC):
 *   "00, OK,00,00\r"  — success, normal response
 *   "39, BLOCKS FREE.\r" — block count in first part
 *
 * The drive status for free blocks is returned by "B-E" command:
 *   sends "01\r02\r<free_lo>\r<free_hi>\r<total_lo>\r<total_hi>\r"
 *
 * This is a simplified parser that reads disk_status() error message
 * which contains block info when formatted properly. */
static bbs_err_t parse_blocks(u8 device, u16 *out_blocks, u8 is_free)
{
    u8 code = disk_status(device);

    /* disk_status reads into disk_errmsg and returns error code.
     * For free blocks, we parse the message. Format varies by drive type.
     * Standard approach: send "B-E" command and parse block count. */

    /* If status is 0 (OK) and we can parse the block count from errmsg... */
    if (code >= 20) {
        return BBS_EIO;  /* Disk error */
    }

    /* Parse block count from disk_errmsg (simplified).
     * Real implementation would be more sophisticated, but for now
     * we return 0 (indicating data not available) and defer to
     * a more complete block-reading implementation. */
    *out_blocks = 0;
    return BBS_OK;
}

/**
 * disk_free_blocks()
 *
 * Get disk free space in 254-byte blocks.
 * Note: Full implementation requires more sophisticated CBM DOS parsing.
 * This placeholder returns 0 (unknown) but can be extended.
 */
bbs_err_t disk_free_blocks(u8 device, u16 *out_free)
{
    if (!out_free) {
        return BBS_EBADARG;
    }
    /* Placeholder: return 0 blocks (unknown).
     * Real implementation would parse CBM DOS block info. */
    *out_free = 0;
    return BBS_OK;
}

/**
 * disk_total_blocks()
 *
 * Get disk total capacity in 254-byte blocks.
 * This is typically 3160 for 1581 (800KB), 683 for 1571 (170KB).
 */
bbs_err_t disk_total_blocks(u8 device, u16 *out_total)
{
    if (!out_total) {
        return BBS_EBADARG;
    }
    /* Placeholder: return safe default (3160 for 1581).
     * Real implementation would detect drive type. */
    *out_total = 3160;  /* 1581 default */
    return BBS_OK;
}
