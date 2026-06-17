#!/usr/bin/env python3
"""Inject a one-record DOORS REL file into a .d81 image so the DOOR PROGRAMS
feature is pre-registered (zero manual CONFIGURE).  Creates a CBM 1581 REL
file named "DOORS" with record length 40 holding a single door:
  FORTUNE — device 8, drive 0, key 'F', enabled.

No-op if a "DOORS" file already exists (preserves real sysop registrations).
The REL layout mirrors what CBM DOS would create for one written record:
one data block (record 1) + one side sector, with a directory entry of type
REL (0x84).  Empty slots 2..DOORS_MAX simply don't exist on disk yet, so the
BBS reads short/EOF for them (door_count/door_by_id handle that).

Usage: seed-doors.py <image.d81>
"""
import sys

SPT = 40                      # sectors per track (1581)
BPS = 256                     # bytes per sector
DIR_TRACK = 40
DIR_FIRST_SEC = 3
RECLEN = 40                   # door_record_t on-disk size (RECORD_SIZE_DOOR)


def off(t, s):
    return (t - 1) * SPT * BPS + s * BPS


def fortune_record():
    r = bytearray(RECLEN)
    r[0] = 1                  # id
    r[1] = 0x01               # flags: DOOR_F_ENABLED
    name = b"FORTUNE"
    r[2:2 + len(name)] = name           # title[16] (NUL-padded)
    r[18:18 + len(name)] = name         # filename[16] (NUL-padded)
    r[34] = 8                 # device
    r[35] = 0                 # drive
    r[36] = ord('F')          # cmd_key (uppercase)
    r[37] = 0                 # min_level
    r[38] = 0                 # login_order
    # r[39] reserved = 0
    return bytes(r)


def find_doors(img):
    """Return True if a DOORS file already exists in the directory."""
    t, s = DIR_TRACK, DIR_FIRST_SEC
    seen = set()
    while t and (t, s) not in seen:
        seen.add((t, s))
        o = off(t, s)
        sec = img[o:o + BPS]
        if len(sec) < BPS:
            break
        nt, ns = sec[0], sec[1]
        for base in range(2, BPS, 32):
            if (sec[base] & 0x07) == 0 and sec[base] != 0:
                continue
            nm = bytes(b & 0x7f for b in sec[base + 3:base + 19]).rstrip(b'\x00\x20')
            if nm.upper() == b'DOORS' and (sec[base] & 0x0f) != 0:
                return True
        t, s = nt, ns
    return False


def bam_alloc(img, n):
    """Allocate n free blocks from tracks 1..39 (avoid the dir track 40).
    1581 BAM: T40S1 covers tracks 1-40, each track entry = 6 bytes
    (1 free-count + 5 bitmap bytes; bit set = free) at offset 16+(t-1)*6.
    Returns list of (track, sector); marks them used. Raises on no space."""
    bam_o = off(DIR_TRACK, 1)
    out = []
    for t in range(1, DIR_TRACK):           # tracks 1..39
        eo = bam_o + 16 + (t - 1) * 6
        free = img[eo]
        if free == 0:
            continue
        for s in range(SPT):
            byte = eo + 1 + (s // 8)
            bit = 1 << (s % 8)
            if img[byte] & bit:              # free
                img[byte] &= ~bit            # mark used
                img[eo] -= 1
                out.append((t, s))
                if len(out) == n:
                    return out
    raise RuntimeError("not enough free blocks in d81 to seed DOORS")


def free_dir_slot(img):
    """Find a free directory slot (type byte == 0). Returns (sector_off, base)."""
    t, s = DIR_TRACK, DIR_FIRST_SEC
    seen = set()
    while t and (t, s) not in seen:
        seen.add((t, s))
        o = off(t, s)
        for base in range(2, BPS, 32):
            if img[o + base] == 0:           # unused entry
                return o, base
        t, s = img[o + 0], img[o + 1]
    raise RuntimeError("no free directory slot for DOORS")


def main():
    path = sys.argv[1]
    with open(path, 'r+b') as f:
        img = bytearray(f.read())
    if len(img) < 819200:
        print(f"  seed-doors: image too small ({len(img)} bytes) — skipping", file=sys.stderr)
        return
    if find_doors(img):
        print("  seed-doors: DOORS already present — leaving it untouched")
        return

    (dt, ds), (st, ss) = bam_alloc(img, 2)   # data block, side sector

    # Data block: link [0-1] = (0, last-used-byte-index); records start at byte 2.
    rec = fortune_record()
    do = off(dt, ds)
    img[do:do + BPS] = bytes(BPS)
    img[do + 0] = 0                          # track 0 => last block in chain
    img[do + 1] = 2 + RECLEN - 1             # index of last used byte (=41)
    img[do + 2:do + 2 + RECLEN] = rec

    # Side sector: [0-1] next SS (none), [2] SS#=0, [3] reclen, [4-5] this SS T/S,
    # [16-17] first data block T/S.
    so = off(st, ss)
    img[so:so + BPS] = bytes(BPS)
    img[so + 0] = 0
    img[so + 1] = 0
    img[so + 2] = 0
    img[so + 3] = RECLEN
    img[so + 4] = st
    img[so + 5] = ss
    img[so + 16] = dt
    img[so + 17] = ds

    # Directory entry: REL (0x84), data T/S, name, side-sector T/S, reclen, blocks.
    dofs, base = free_dir_slot(img)
    e = dofs + base
    img[e + 0] = 0x84                        # closed REL
    img[e + 1] = dt
    img[e + 2] = ds
    nm = b"DOORS".ljust(16, b'\xa0')
    img[e + 3:e + 19] = nm
    img[e + 19] = st
    img[e + 20] = ss
    img[e + 21] = RECLEN
    img[e + 22:e + 28] = bytes(6)
    img[e + 28] = 2                          # block count lo (2 blocks)
    img[e + 29] = 0                          # block count hi

    with open(path, 'r+b') as f:
        f.write(bytes(img))
    print(f"  seed-doors: wrote DOORS REL (data {dt}/{ds}, side {st}/{ss}) — FORTUNE registered")


if __name__ == "__main__":
    main()
