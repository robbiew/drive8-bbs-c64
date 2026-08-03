#!/usr/bin/env bash
# Build, deploy, and (best-effort) launch T/64 on one of three test targets.
#
# Usage: tools/deploy.sh <d81|siec|uiec> [options]
#
# Targets:
#   d81   Device 8, the emulated 1581. Assembles a SEEDED disk image (the
#         seed is required — an unseeded disk has no USR LOG and
#         boot_sequence() correctly refuses to start) and mounts it.
#   siec  Device 11, SoftIEC. Runs tools/migrate-d81.py to build the folder
#         tree, then uploads it to the device's Default Path over FTP.
#   uiec  Device 10, a physical uIEC/sd2iec. Not reachable over the network:
#         stages device 8 (same as the d81 target) and drives COPYALL
#         through the C64 to copy the program set across. See NOTES below.
#
# Options (all targets):
#   --execute            Actually call c64u / upload. Default: DRY RUN —
#                         every c64u invocation is printed, not run. This is
#                         deliberate: review the printed commands before
#                         pointing this at real hardware.
#   --launch             After deploying, attempt a best-effort launch (see
#                         per-target notes below). The exact manual fallback
#                         is always printed, whether or not this is passed.
#   --no-build           Skip the `make` step; deploy what's already built.
#   -h, --help           Show this message.
#
# d81 options:
#   --seed <d81>          Seed disk for the user DB (default: data/users-seed.d81)
#   --drive <a|b>         Internal drive slot to mount into (default: a)
#
# siec options:
#   --device <n>          SoftIEC bus id (default: 11; env T64_SIEC_DEVICE)
#   --base <path>         Default Path on the Ultimate (default: /USB1/TURBO64;
#                          env T64_SIEC_BASE)
#
# uiec options:
#   --uiec-device <n>     Physical drive's device number (default: 10; env
#                          T64_UIEC_DEVICE) — informational only: this is
#                          src-diag/copyall.c's hardcoded destination, not
#                          something this script can change without touching
#                          C source.
#
# NOTES — read before using --launch:
#
#   `c64u runners run-prg` issues LOAD"<path>",8,1: it FORCES device 8 and
#   TRUNCATES the path to 16 characters. It works for the d81 target (device
#   8 is already correct there) but cannot launch the siec target at all —
#   $BA ends up 8 and the BBS reports OVL_BOOT LOAD FAILED. For siec,
#   `machine sendkey` is the only route found so far, and it is unreliable:
#   it can drop the trailing RETURN when a line is chunked, so the text and
#   the '\n' are sent as two separate calls here. If it does not take,
#   type the two printed lines by hand.
#
#   uiec automation (--launch): COPYALL.prg is itself loaded via run-prg
#   (device 8, so run-prg's forced device is correct), then a bare "C"
#   keystroke starts it (COPYALL waits on getch(), not a BASIC line, so the
#   sendkey chunking bug does not apply). This has NOT been validated
#   against real hardware by this script — treat it as experimental and
#   watch the console. COPYALL itself is a diagnostic under src-diag/, out
#   of scope to modify here; two things about it are worth knowing before
#   relying on it:
#     - its file list hardcodes "BOOT-0.3.1"/"CONFIGURE-0.3.1": if the repo
#       version has moved on since copyall.c was last edited, it silently
#       copies nothing for those two names. This script warns when the
#       versions disagree.
#     - its file list does not include OVL_AUTH, so a device populated via
#       COPYALL will fail login until that overlay is copied by hand.
#
# Environment:
#   T64_SIEC_DEVICE   default SoftIEC bus id (11)
#   T64_SIEC_BASE     default SoftIEC Default Path (/USB1/TURBO64)
#   T64_UIEC_DEVICE   default physical drive device number (10, informational)
#   C1541             path to the VICE c1541 tool (used by migrate-d81.py)
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/vendor/c64u/bin/c64u"
VERSION_COMPACT="$(grep 'BBS_RELEASE_VERSION_COMPACT' "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

show_help() {
    sed -n '2,71p' "$0"
}

TARGET="${1:-}"
shift || true

EXECUTE=0
LAUNCH=0
BUILD=1
D81_SEED="$ROOT/data/users-seed.d81"
D81_DRIVE="a"
SIEC_DEVICE="${T64_SIEC_DEVICE:-11}"
SIEC_BASE="${T64_SIEC_BASE:-/USB1/TURBO64}"
UIEC_DEVICE="${T64_UIEC_DEVICE:-10}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --execute)      EXECUTE=1; shift ;;
        --launch)       LAUNCH=1; shift ;;
        --no-build)     BUILD=0; shift ;;
        --seed)         D81_SEED="$2"; shift 2 ;;
        --drive)        D81_DRIVE="$2"; shift 2 ;;
        --device)       SIEC_DEVICE="$2"; shift 2 ;;
        --base)         SIEC_BASE="$2"; shift 2 ;;
        --uiec-device)  UIEC_DEVICE="$2"; shift 2 ;;
        -h|--help)      show_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

case "$TARGET" in
    d81|siec|uiec) ;;
    -h|--help|"") show_help; exit 0 ;;
    *) echo "Unknown target: $TARGET (expected d81, siec, or uiec)" >&2; exit 1 ;;
esac

if [ ! -x "$BIN" ]; then
    echo "ERROR: c64u not found at $BIN" >&2
    echo "Run 'tools/install-c64u.sh' first." >&2
    exit 1
fi

# Every c64u call goes through here. Dry-run (the default) only prints what
# WOULD run — nothing touches hardware until --execute is passed.
run_c64u() {
    if [ "$EXECUTE" -eq 1 ]; then
        "$BIN" "$@"
    else
        echo "[dry-run] c64u $*"
    fi
}

echo -e "${BLUE}T/64 deploy: target=${TARGET} version=${VERSION_COMPACT} execute=${EXECUTE} launch=${LAUNCH}${NC}"
if [ "$EXECUTE" -eq 0 ]; then
    echo -e "${YELLOW}DRY RUN — no c64u command below will actually execute. Pass --execute to run for real.${NC}"
fi
echo ""

deploy_d81() {
    if [ "$BUILD" -eq 1 ]; then
        echo -e "${BLUE}Building REL binaries...${NC}"
        (cd "$ROOT" && make c64 && make editor)
    fi

    if [ ! -f "$D81_SEED" ]; then
        echo "ERROR: seed disk not found: $D81_SEED" >&2
        echo "The seed is required — a plain 'make disk' produces a disk with no" >&2
        echo "user database, and boot_sequence() correctly refuses to start." >&2
        exit 1
    fi

    echo -e "${BLUE}Assembling seeded disk image...${NC}"
    bash "$ROOT/tools/assemble-d81.sh" --seed-users "$D81_SEED"

    local image="$ROOT/build/c64/TURBO64-${VERSION_COMPACT}.d81"
    echo -e "${BLUE}Mounting to internal drive ${D81_DRIVE} (device 8)...${NC}"
    run_c64u drives mount-upload "$D81_DRIVE" "$image" --type d81 --mode readwrite

    if [ "$LAUNCH" -eq 1 ]; then
        echo -e "${BLUE}Launching (run-prg forces device 8, which is correct here)...${NC}"
        local boot_prg="$ROOT/build/c64/BOOT-${VERSION_COMPACT}.prg"
        local remote="/USB1/T64RUN.PRG"
        run_c64u fs upload "$boot_prg" "$remote"
        run_c64u runners run-prg "$remote"
    fi

    echo ""
    echo -e "${GREEN}d81 deploy done.${NC} Manual boot on the C64, if not using --launch:"
    echo "    LOAD\"BOOT-${VERSION_COMPACT}\",8"
    echo "    RUN"
}

deploy_siec() {
    if [ "$BUILD" -eq 1 ]; then
        echo -e "${BLUE}Building SIEC binaries...${NC}"
        (cd "$ROOT" && make c64-siec && make editor-siec)
    fi

    if [ ! -f "$D81_SEED" ]; then
        echo "ERROR: seed disk not found: $D81_SEED" >&2
        echo "migrate-d81.py needs a seeded .d81 as its data source." >&2
        exit 1
    fi

    echo -e "${BLUE}Assembling seeded disk image (data source for migration)...${NC}"
    bash "$ROOT/tools/assemble-d81.sh" --seed-users "$D81_SEED"
    local image="$ROOT/build/c64/TURBO64-${VERSION_COMPACT}.d81"

    local tree="$ROOT/build/c64/siec-tree"
    echo -e "${BLUE}Building SoftIEC folder tree (tools/migrate-d81.py)...${NC}"
    rm -rf "$tree"
    python3 "$ROOT/tools/migrate-d81.py" "$image" "$tree" \
        --base "$SIEC_BASE" --device "$SIEC_DEVICE"

    echo -e "${BLUE}Uploading tree to ${SIEC_BASE} (device ${SIEC_DEVICE})...${NC}"
    run_c64u fs mkdir "$SIEC_BASE" || true
    for section in SYSTEM MSGS FILES DOORS; do
        run_c64u fs mkdir "$SIEC_BASE/$section" || true
    done
    while IFS= read -r -d '' f; do
        local rel="${f#"$tree"/}"
        run_c64u fs upload "$f" "$SIEC_BASE/$rel"
    done < <(find "$tree" -type f -print0)

    local boot_name="BOOT-${VERSION_COMPACT}-SIEC"
    if [ "$LAUNCH" -eq 1 ]; then
        echo -e "${BLUE}Attempting best-effort launch via sendkey (UNRELIABLE — watch the console)...${NC}"
        run_c64u machine sendkey "LOAD\"${boot_name}\",${SIEC_DEVICE}"
        run_c64u machine sendkey '\n'
        sleep 1
        run_c64u machine sendkey 'RUN'
        run_c64u machine sendkey '\n'
    fi

    echo ""
    echo -e "${GREEN}siec deploy done.${NC} Manual boot on the C64 (reliable path):"
    echo "    LOAD\"${boot_name}\",${SIEC_DEVICE}"
    echo "    RUN"
    echo "(if sendkey drops the RETURN, type these by hand — send the text and"
    echo " '\\n' as separate keystrokes if scripting it again)"
    echo ""
    echo -e "${YELLOW}Reminder: the SoftIEC current directory persists across a reset.${NC}"
    echo "This script only uploads over FTP (does not touch IEC bus DOS state), so"
    echo "it never leaves the drive cursor positioned anywhere — only a program run"
    echo "ON the C64 (the BBS itself, COPYALL, diagnostics) can do that. If you run"
    echo "anything else on the C64 against device ${SIEC_DEVICE} afterwards, follow"
    echo "the commit e57b968 precedent: CD back to ${SIEC_BASE} before it exits."
}

deploy_uiec() {
    echo -e "${YELLOW}uiec (device ${UIEC_DEVICE}) has no network path — its media is not${NC}"
    echo -e "${YELLOW}reachable from this script. Staging device 8, then driving COPYALL${NC}"
    echo -e "${YELLOW}through the C64 to copy the program set across (src-diag/copyall.c).${NC}"
    echo ""

    deploy_d81

    if [ "$BUILD" -eq 1 ]; then
        echo -e "${BLUE}Building diagnostics (COPYALL)...${NC}"
        (cd "$ROOT" && make diag)
    fi

    local copyall_versions
    copyall_versions="$(grep -o '"BOOT-[0-9.]*"' "$ROOT/src-diag/copyall.c" | tr -d '"' | sed 's/^BOOT-//')"
    if [ "$copyall_versions" != "$VERSION_COMPACT" ]; then
        echo -e "${RED}WARNING: src-diag/copyall.c hardcodes version ${copyall_versions}," \
                 "but this build is ${VERSION_COMPACT}.${NC}"
        echo -e "${RED}COPYALL will silently skip BOOT-${VERSION_COMPACT} and CONFIGURE-${VERSION_COMPACT}" \
                 "(it does exact-name matches). This is a copyall.c issue, out of scope here" \
                 "(no C source changes) — fix the hardcoded names there before relying on it.${NC}"
    fi
    echo -e "${YELLOW}NOTE: copyall.c's file list also does not include OVL_AUTH — the copy" \
             "will be missing the login overlay until that is added by hand.${NC}"
    echo ""

    local copyall_prg="$ROOT/build/c64/COPYALL.prg"
    if [ "$LAUNCH" -eq 1 ]; then
        echo -e "${BLUE}Attempting best-effort launch of COPYALL (EXPERIMENTAL, unverified on hardware)...${NC}"
        local remote="/USB1/T64COPYALL.PRG"
        run_c64u fs upload "$copyall_prg" "$remote"
        run_c64u runners run-prg "$remote"
        sleep 1
        run_c64u machine sendkey "C"
        echo "Watch the console for 'DONE. N FAILED.' — this script cannot read it back."
    fi

    echo ""
    echo -e "${GREEN}uiec staging done.${NC} On the C64 (device 8 is now the seeded d81):"
    echo "    LOAD\"COPYALL\",8"
    echo "    RUN"
    echo "    (press C to start; it copies device 8 -> device ${UIEC_DEVICE} partition 1)"
}

case "$TARGET" in
    d81)  deploy_d81 ;;
    siec) deploy_siec ;;
    uiec) deploy_uiec ;;
esac
