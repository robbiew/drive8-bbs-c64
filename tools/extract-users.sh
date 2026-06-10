#!/usr/bin/env bash
# Fetch the live D8 disk from U64 and extract user database files.
#
# Saves:
#   data/users-seed.d81  — Seed disk image with user data preserved
#   data/usr_log         — USR LOG raw binary (flat record bytes, no CBM overhead)
#   data/usr_prof        — USR PROF raw binary (flat record bytes)
#
# Usage: tools/extract-users.sh [options]
#   -l, --location <loc>  Source location: usb1 (default), sd, or full path
#   -d, --disk <path>     Use an existing local .d81 instead of fetching
#   -h, --help
#
# When bundled to data/, assemble-d81.sh will automatically use
# data/users-seed.d81 as the seed image for fresh builds.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DATA_DIR="$ROOT/data"
LOCATION=""
DISK=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -l|--location) LOCATION="$2"; shift 2 ;;
        -d|--disk)     DISK="$2";     shift 2 ;;
        -h|--help)     sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# Needed by both the fetch path (output filename) and Step 3 (PRG stripping),
# so resolve it up front — not only on the fetch branch.
VERSION_COMPACT="$(grep 'BBS_RELEASE_VERSION_COMPACT' \
    "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"

# -- Step 1: obtain the .d81 ------------------------------------------
if [ -n "$DISK" ]; then
    if [ ! -f "$DISK" ]; then
        echo "ERROR: disk not found: $DISK" >&2; exit 1
    fi
    D81="$DISK"
else
    FETCH_OUT="$ROOT/build/c64/TURBO64-${VERSION_COMPACT}-live.d81"
    echo "Fetching live disk from U64..."
    FETCH_ARGS=()
    [ -n "$LOCATION" ] && FETCH_ARGS+=(-l "$LOCATION")
    bash "$ROOT/tools/fetch-u64.sh" ${FETCH_ARGS[@]+"${FETCH_ARGS[@]}"} -o "$FETCH_OUT"
    D81="$FETCH_OUT"
fi

echo "Source disk: $D81"

# -- Step 2: extract USR LOG and USR PROF via Python ------------------
python3 << PYSCRIPT
import struct, sys, os

DISK  = "$D81"
DDIR  = "$DATA_DIR"

with open(DISK, 'rb') as f:
    img = f.read()

def sector(track, s):
    return img[(track - 1) * 40 * 256 + s * 256:
               (track - 1) * 40 * 256 + s * 256 + 256]

def read_chain(first_track, first_sector):
    """Follow a CBM sector chain and return the raw data bytes."""
    raw = bytearray()
    t, s = first_track, first_sector
    while t != 0:
        sec = sector(t, s)
        next_t, next_s = sec[0], sec[1]
        if next_t == 0:
            # Last sector: next_s = index of last used byte (inclusive)
            raw += sec[2:next_s + 1]
        else:
            raw += sec[2:256]
        t, s = next_t, next_s
    return bytes(raw)

def find_rel_files(target_names):
    """Scan directory chain and return {name: (first_data_t, first_data_s, reclen)}."""
    results = {}
    t, s = 40, 3    # D81 directory at T40 S3
    while t != 0:
        sec = sector(t, s)
        t, s = sec[0], sec[1]
        for offset in range(2, 256, 32):
            entry = sec[offset:offset + 32]
            # The last slot in a sector (offset 226) is only 30 bytes, but a
            # directory entry is fully described in its first 22 bytes (reclen
            # is at index 21).  Requiring 32 here silently dropped the last
            # entry of every sector — which is exactly where USR LOG often lands.
            if len(entry) < 22:
                break
            ftype = entry[0] & 0x07
            if ftype != 4:      # 4 = REL
                continue
            fname = bytes(b & 0x7f for b in entry[3:19]).rstrip(b'\xa0').rstrip(b'\x00')
            fname_str = fname.decode('ascii', errors='replace').strip()
            if fname_str.upper() in target_names:
                data_t, data_s = entry[1], entry[2]
                reclen = entry[21]
                results[fname_str.upper()] = (data_t, data_s, reclen)
    return results

targets = {"USR LOG", "USR PROF"}
found   = find_rel_files(targets)

if not found:
    print("ERROR: No user REL files found in disk directory.", file=sys.stderr)
    sys.exit(1)

for name, (dt, ds, reclen) in sorted(found.items()):
    raw  = read_chain(dt, ds)
    recs = len(raw) // reclen if reclen else 0
    print(f"  {name}: {len(raw)} bytes, {recs} records × {reclen}B")

    out_name = name.lower().replace(' ', '_')
    out_path = os.path.join(DDIR, out_name)
    with open(out_path, 'wb') as fout:
        fout.write(raw)
    print(f"    → {out_path}")

missing = targets - set(found)
if missing:
    print(f"  WARNING: not found on disk: {', '.join(sorted(missing))}", file=sys.stderr)
PYSCRIPT

# -- Step 3: save the seed disk ---------------------------------------
SEED_OUT="$DATA_DIR/users-seed.d81"
cp "$D81" "$SEED_OUT"

# Strip boot/editor PRGs so the seed stays focused on user data.
for name in d8boot-"$VERSION_COMPACT" d8edit-"$VERSION_COMPACT" boot-"$VERSION_COMPACT" configure-"$VERSION_COMPACT"; do
    c1541 "$SEED_OUT" -delete "$name" >/dev/null 2>&1 || true
done
echo "  → $SEED_OUT"

echo ""
echo "✓ Done. Run 'make disk' to build a seeded disk image."
echo "  (assemble-d81.sh will auto-use data/users-seed.d81 when present)"
