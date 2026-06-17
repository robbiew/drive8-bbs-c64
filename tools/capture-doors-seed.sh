#!/usr/bin/env bash
# Capture a built disk (with a DOORS table registered via CONFIGURE) into the
# doors seed, so `build.sh ... --seed-doors` carries the door programs forward
# and you never have to re-register them.
#
# The DOORS table is a real CBM REL created by the BBS; we snapshot the whole
# .d81 (the same way data/users-seed.d81 is a snapshot).  --seed-doors then uses
# it as the assemble base, and assemble preserves its REL files (USR LOG/PROF +
# DOORS) while overwriting the PRGs with freshly built ones.
#
# Usage:
#   tools/capture-doors-seed.sh [path/to/disk.d81]
# Default source: build/c64/TURBO64-<ver>.d81  (your current working disk)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_COMPACT="$(grep 'BBS_RELEASE_VERSION_COMPACT' "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"
SRC="${1:-$ROOT/build/c64/TURBO64-${VERSION_COMPACT}.d81}"
DEST="$ROOT/data/doors-seed.d81"
C1541="${C1541:-c1541}"

if [ ! -f "$SRC" ]; then
    echo "ERROR: source disk not found: $SRC" >&2
    exit 1
fi

# Require a DOORS REL on the source, or the capture is pointless.
if ! "$C1541" "$SRC" -dir 2>/dev/null | grep -qiE '"doors"[[:space:]]+rel'; then
    echo "ERROR: no DOORS REL on $SRC." >&2
    echo "       Register a door in CONFIGURE > DOOR PROGRAMS first, then re-run." >&2
    exit 1
fi

cp "$SRC" "$DEST"
echo "Captured doors seed: $DEST"
"$C1541" "$DEST" -dir 2>/dev/null | grep -iE '"doors"[[:space:]]+rel|"usr log"' || true
echo "Now build with --seed-doors to carry the door table forward:"
echo "  tools/build.sh vice --seed-doors"
