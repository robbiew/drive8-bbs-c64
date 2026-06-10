#!/usr/bin/env bash
# Fetch BOARDS.D81 from Ultimate64 and save as data/boards-seed.d81
#
# BOARDS.D81 is the drive-9 message-board disk: it holds the BOARDS DIR REL
# file and any B<n>.IDX / B<n>.TXT files for each board.  Seeding it here
# lets you quickly wipe-and-restore the message area during testing without
# losing your baseline board configuration.
#
# Usage: tools/extract-boards.sh [options]
#   -l, --location <loc>  Source location on U64: bbs (default → /BBS), usb0, usb1, sd, or full path
#   -d, --disk <path>     Use an existing local .d81 instead of fetching from U64
#   -h, --help
#
# To restore the saved seed back to U64:
#   tools/deploy-u64.sh --boards [-l bbs]
#
# Environment:
#   T64_SD_PATH            Override U64 source base path (default: bbs → /BBS)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/vendor/c64u/bin/c64u"
DATA_DIR="$ROOT/data"
C1541="${C1541:-c1541}"
VERSION_COMPACT="$(grep 'BBS_RELEASE_VERSION_COMPACT' "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"
LOCATION=""
DISK=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -l|--location)
            LOCATION="$2"
            shift 2
            ;;
        -d|--disk)
            DISK="$2"
            shift 2
            ;;
        -h|--help)
            sed -n '2,15p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

LOCATION="${LOCATION:-${T64_SD_PATH:-bbs}}"
case "$LOCATION" in
    bbs)  SD_PATH="/BBS"       ;;
    usb0) SD_PATH="/USB0/BBS"  ;;
    usb1) SD_PATH="/USB1/BBS"  ;;
    sd)   SD_PATH="/SD/BBS"    ;;
    *)    SD_PATH="$LOCATION"  ;;
esac
SD_PATH="${SD_PATH%/}"

SEED_OUT="$DATA_DIR/boards-seed.d81"

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║            Fetching BOARDS disk from Ultimate64                ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# -- Obtain disk ----------------------------------------------------------
if [ -n "$DISK" ]; then
    if [ ! -f "$DISK" ]; then
        echo "ERROR: Disk not found: $DISK" >&2
        exit 1
    fi
    D81="$DISK"
    echo "Using local disk: $D81"
    echo ""
else
    if [ ! -x "$BIN" ]; then
        echo "ERROR: c64u not found at $BIN" >&2
        echo "Run 'tools/install-c64u.sh' first." >&2
        exit 1
    fi
    BOARDS_NAME="BOARDS-${VERSION_COMPACT}.D81"
    REMOTE_PATH="${SD_PATH}/${BOARDS_NAME}"
    D81="${TMPDIR:-/tmp}/boards-$$.d81"
    trap 'rm -f "$D81"' EXIT

    echo "Source:   $REMOTE_PATH"
    echo "Dest:     $SEED_OUT"
    echo ""
    "$BIN" fs download "$REMOTE_PATH" "$D81" || {
        echo "  ✗ Failed to download ${BOARDS_NAME}" >&2
        exit 1
    }
    echo "  ✓ Downloaded"
    echo ""
fi

# -- Save seed ------------------------------------------------------------
cp "$D81" "$SEED_OUT"
echo "  → $SEED_OUT"
echo ""

# Show disk contents
"$C1541" "$SEED_OUT" -list 2>/dev/null | head -30 || true
echo ""
echo "✓ Done."
echo "  To restore this seed to U64: tools/deploy-u64.sh --boards [-l <loc>]"
