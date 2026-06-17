#!/usr/bin/env bash
# Capture a disk that has a DOORS table registered via CONFIGURE into the doors
# seed, so `build.sh ... --seed-doors` carries the door programs forward and you
# never have to re-register them.
#
# The DOORS table is a real CBM REL created by the BBS; we snapshot the whole
# .d81 (the same way data/users-seed.d81 is a snapshot).  --seed-doors then uses
# it as the assemble base, preserving its REL files (USR LOG/PROF + DOORS) while
# overwriting the PRGs with freshly built ones.
#
# Sources:
#   (default)  the local VICE disk:  build/c64/TURBO64-<ver>.d81
#   <path>     any local .d81 you pass as the first argument
#   --from-u64 [-l usb1|sd]  fetch the live disk from an Ultimate64 first
#
# Usage:
#   tools/capture-doors-seed.sh                 # from the VICE working disk
#   tools/capture-doors-seed.sh path/to.d81     # from a specific local image
#   tools/capture-doors-seed.sh --from-u64      # from the U64 (/USB1/BBS)
#   tools/capture-doors-seed.sh --from-u64 -l sd
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_COMPACT="$(grep 'BBS_RELEASE_VERSION_COMPACT' "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"
DEST="$ROOT/data/doors-seed.d81"
C1541="${C1541:-c1541}"

FROM_U64=0
LOCATION="usb1"
SRC=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --from-u64) FROM_U64=1; shift ;;
        -l|--location) LOCATION="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) SRC="$1"; shift ;;
    esac
done

if [ "$FROM_U64" -eq 1 ]; then
    # Pull the live disk off the Ultimate64, then capture from it.
    SRC="$ROOT/build/c64/TURBO64-${VERSION_COMPACT}-live.d81"
    echo "Fetching live disk from U64 ($LOCATION)..."
    bash "$ROOT/tools/fetch-u64.sh" -l "$LOCATION" -o "$SRC"
elif [ -z "$SRC" ]; then
    SRC="$ROOT/build/c64/TURBO64-${VERSION_COMPACT}.d81"   # local VICE working disk
fi

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
echo "Captured doors seed: $DEST  (from $(basename "$SRC"))"
"$C1541" "$DEST" -dir 2>/dev/null | grep -iE '"doors"[[:space:]]+rel|"usr log"' || true
echo "Now build with --seed-doors to carry the door table forward:"
echo "  tools/build.sh vice --seed-doors      # or:  tools/build.sh u64 --seed-doors"
