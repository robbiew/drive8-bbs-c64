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

    # A fully-formed DOORS_MAX-record table: record 1 = FORTUNE, records 2..16
    # all-zero (id 0 => empty).  Seeding ALL records (not just one) means the
    # BBS reads 16 distinct records on a sequential scan regardless of how its
    # DOS signals end-of-file on this hand-built REL — empty slots read id 0 and
    # are skipped, so the menu shows exactly one door.
    DOORS_MAX = 16
    DATA_BYTES = BPS - 2                       # 254 record-bytes per data block
    stream = bytearray(fortune_record())       # record 1
    stream += bytes(RECLEN * (DOORS_MAX - 1))  # records 2..16 = empty
    nblocks = (len(stream) + DATA_BYTES - 1) // DATA_BYTES   # 640/254 -> 3

    blocks = bam_alloc(img, nblocks + 1)       # n data blocks + 1 side sector
    data_ts = blocks[:nblocks]
    st, ss = blocks[nblocks]

    # Write the data blocks, chaining each to the next.
    for i, (dt, dsx) in enumerate(data_ts):
        o = off(dt, dsx)
        img[o:o + BPS] = bytes(BPS)
        chunk = stream[i * DATA_BYTES:(i + 1) * DATA_BYTES]
        if i + 1 < nblocks:                    # not last: link to next block
            nt, ns = data_ts[i + 1]
            img[o + 0] = nt
            img[o + 1] = ns
        else:                                  # last block: track 0, last byte index
            img[o + 0] = 0
            img[o + 1] = 2 + len(chunk) - 1
        img[o + 2:o + 2 + len(chunk)] = chunk

    # Side sector: [0-1] next SS (none), [2] SS#=0, [3] reclen, [4-5] this SS T/S,
    # [16..] track/sector pairs of every data block.
    so = off(st, ss)
    img[so:so + BPS] = bytes(BPS)
    img[so + 3] = RECLEN
    img[so + 4] = st
    img[so + 5] = ss
    for i, (dt, dsx) in enumerate(data_ts):
        img[so + 16 + i * 2] = dt
        img[so + 17 + i * 2] = dsx

    # Directory entry: REL (0x84), first data T/S, name, side-sector T/S, reclen, blocks.
    dofs, base = free_dir_slot(img)
    e = dofs + base
    img[e + 0] = 0x84                          # closed REL
    img[e + 1], img[e + 2] = data_ts[0]
    img[e + 3:e + 19] = b"DOORS".ljust(16, b'\xa0')
    img[e + 19] = st
    img[e + 20] = ss
    img[e + 21] = RECLEN
    img[e + 22:e + 28] = bytes(6)
    total_blocks = nblocks + 1
    img[e + 28] = total_blocks & 0xFF
    img[e + 29] = total_blocks >> 8

    with open(path, 'r+b') as f:
        f.write(bytes(img))
    print(f"  seed-doors: wrote DOORS REL ({DOORS_MAX} records, {nblocks} data + 1 side) — FORTUNE registered")


if __name__ == "__main__":
    main()
