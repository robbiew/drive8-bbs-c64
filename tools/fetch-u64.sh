#!/usr/bin/env bash
# Fetch the live .d81 disk image from Ultimate64 storage
#
# Downloads the current TURBO64-<version>.D81 from the U64 to the project root,
# overwriting the local copy. Useful for inspecting the live disk after
# running CONFIGURE on the real hardware.
#
# Usage: tools/fetch-u64.sh [options]
#   -l, --location <loc>  Source location: usb1 (default), sd, or full path
#   -o, --output <path>   Local destination (default: <root>/TURBO64-<version>.D81)
#
# Environment:
#   T64_SD_PATH            Override source path (default: /USB1/BBS)
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/vendor/c64u/bin/c64u"
VERSION_COMPACT="$(grep 'BBS_RELEASE_VERSION_COMPACT' "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"

LOCATION=""
OUTPUT=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -l|--location)
            LOCATION="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "  -l, --location <loc>  usb1 (default), sd, or full path"
            echo "  -o, --output <path>   Local destination path"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Determine source path
LOCATION="${LOCATION:-${T64_SD_PATH:-usb1}}"
case "$LOCATION" in
    usb1) SD_PATH="/USB1/BBS" ;;
    sd)   SD_PATH="/SD/BBS"   ;;
    *)    SD_PATH="$LOCATION" ;;
esac
SD_PATH="${SD_PATH%/}"

DISK_NAME="TURBO64-${VERSION_COMPACT}.D81"
REMOTE_PATH="${SD_PATH}/${DISK_NAME}"
LOCAL_PATH="${OUTPUT:-$ROOT/${DISK_NAME}}"

# Verify c64u is available
if [ ! -x "$BIN" ]; then
    echo "ERROR: c64u not found at $BIN" >&2
    echo "Run 'tools/install-c64u.sh' first." >&2
    exit 1
fi

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║              Fetching D8 BBS disk from Ultimate64              ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Source:   ${REMOTE_PATH}"
echo "Dest:     ${LOCAL_PATH}"
echo "Version:  ${VERSION_COMPACT}"
echo ""

"$BIN" fs download "$REMOTE_PATH" "$LOCAL_PATH" || {
    echo "  ✗ Failed to download ${DISK_NAME}" >&2
    exit 1
}

echo "  ✓ ${DISK_NAME} → ${LOCAL_PATH}"
echo ""
echo "✓ Fetch complete!"
