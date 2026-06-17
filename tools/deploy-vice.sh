#!/usr/bin/env bash
# Launches VICE emulator with TURBO/64 BBS disk image, tcpser modem bridge,
# and DS12C887 RTC cartridge at $D700 (ACIA at $DE00, so $D700 avoids conflict)
#
# Usage: tools/deploy-vice.sh [options]
#   -f, --fullscreen    Launch in fullscreen mode
#   -p, --paused        Pause on startup
#   -d, --debugger      Enable debugger window
#   --no-autostart      Don't autostart (manual boot)
#   -c, --config <path> Use custom VICE configuration
#   --no-tcpser         Skip tcpser modem bridge
#   --tcpser-port <p>   Telnet port for tcpser (default: 6400)
#   --jiffydos          Boot with JiffyDOS ROMs (C64 kernal + 1581 drive)
#
# Environment:
#   VICE_CMD            Override VICE command (default: x64sc)
#   TCPSER_CMD          Override tcpser command (default: tcpser)
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_COMPACT="$(grep 'BBS_RELEASE_VERSION_COMPACT' "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"
DISK_IMAGE="$ROOT/build/c64/TURBO64-${VERSION_COMPACT}.d81"

# Check for required dependencies
VICE_CMD="${VICE_CMD:-x64sc}"
if ! command -v "$VICE_CMD" &>/dev/null; then
    echo "ERROR: VICE emulator not found ($VICE_CMD)" >&2
    echo "Install VICE or set VICE_CMD environment variable" >&2
    exit 1
fi

TCPSER_CMD="${TCPSER_CMD:-tcpser}"

# Parse arguments
FULLSCREEN=0
PAUSED=0
DEBUGGER=0
AUTOSTART=1
CONFIG=""
BUILD=1
SEED=1
SEED_FILE="$ROOT/data/users-seed.d81"
USE_TCPSER=1
JIFFYDOS=0
JIFFYDOS_DIR="$ROOT/reference/jiffydos"
JIFFYDOS_KERNAL="$JIFFYDOS_DIR/JiffyDOS_C64_6.01.bin"
JIFFYDOS_1581="$JIFFYDOS_DIR/JiffyDOS_1581.bin"
TCPSER_PORT=6400
TCPSER_VPORT=25232
TCPSER_TRACE=0
TCPSER_LOG="/tmp/tcpser-trace.log"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -f|--fullscreen)
            FULLSCREEN=1
            shift
            ;;
        -p|--paused)
            PAUSED=1
            shift
            ;;
        -d|--debugger)
            DEBUGGER=1
            shift
            ;;
        --no-autostart)
            AUTOSTART=0
            shift
            ;;
        --no-build)
            BUILD=0
            shift
            ;;
        --fresh-users)
            SEED=0
            shift
            ;;
        --seed-users)
            SEED=1   # explicit; this is already the default
            shift
            ;;
        --no-tcpser)
            USE_TCPSER=0
            shift
            ;;
        --jiffydos)
            JIFFYDOS=1
            shift
            ;;
        --tcpser-port)
            TCPSER_PORT="$2"
            shift 2
            ;;
        --trace)
            TCPSER_TRACE=1
            shift
            ;;
        -c|--config)
            CONFIG="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "  -f, --fullscreen         Fullscreen mode"
            echo "  -p, --paused             Pause on startup"
            echo "  -d, --debugger           Enable debugger"
            echo "  --no-autostart           Don't autostart"
            echo "  --no-build               Skip rebuild+assemble; boot existing disk"
            echo "  --fresh-users            Assemble a FRESH disk (no USR LOG/PROF;"
            echo "                           boot halts until you run CONFIGURE's INIT)"
            echo "  -c, --config <path>      Custom config file"
            echo "  --no-tcpser              Skip tcpser modem bridge"
            echo "  --tcpser-port <port>     Telnet port for tcpser (default: 6400)"
            echo "  --trace                  Log tcpser serial byte trace to $TCPSER_LOG"
            echo "  --jiffydos               Boot with JiffyDOS ROMs (C64 kernal + 1581 drive)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Rebuild PRGs + reassemble the disk so VICE always boots the latest source.
# Without this, editing source and re-running this script silently boots a
# stale .d81 (this script only launches the image; it does not build it).
#
# A FRESH disk has no USR LOG/USR PROF/CALLERS (those are created by CONFIGURE's
# INIT), so the BBS halts at boot ("USR LOG NOT FOUND"). By default we seed
# those from data/users-seed.d81 (the same snapshot the U64 --seed-users path
# uses) so VICE boots straight to WFC. Pass --fresh-users to assemble without
# the seed and exercise the INIT-required path instead.
# Pass --no-build to skip building entirely and boot whatever disk exists.
if [ $BUILD -eq 1 ]; then
    echo "Building (use --no-build to skip)..."
    if ! make -C "$ROOT" all; then
        echo "ERROR: build failed" >&2
        exit 1
    fi

    # Apply VICE-specific config overrides (deltas in data/config.vice) on top of
    # the built config so VICE gets MODEM_TYPE=VICE / its own displayed baud while
    # data/config stays the single source of truth for hardware. Replaces matching
    # KEY= lines, appends any new ones; '#'/blank lines in the override are ignored.
    VICE_CFG="$ROOT/data/config.vice"
    BUILT_CFG="$ROOT/build/c64/config"
    if [ -f "$VICE_CFG" ] && [ -f "$BUILT_CFG" ]; then
        echo "  Applying VICE config overrides ($(basename "$VICE_CFG"))"
        awk -F= '
            FNR==NR { if ($0 ~ /^[A-Za-z_]+=/) { ov[$1]=$0 } next }
            { if ($1 in ov) { print ov[$1]; seen[$1]=1 } else print }
            END { for (k in ov) if (!(k in seen)) print ov[k] }
        ' "$VICE_CFG" "$BUILT_CFG" > "$BUILT_CFG.tmp" && mv "$BUILT_CFG.tmp" "$BUILT_CFG"
    fi

    echo "Assembling disk image..."
    if [ $SEED -eq 1 ] && [ -f "$SEED_FILE" ]; then
        echo "  Seeding USR LOG/PROF from $(basename "$SEED_FILE") (--fresh-users to skip)"
        "$ROOT/tools/assemble-d81.sh" --seed-users "$SEED_FILE"
    else
        if [ $SEED -eq 1 ]; then
            echo "  NOTE: $SEED_FILE not found — FRESH disk; boot will halt until CONFIGURE INIT" >&2
        else
            echo "  --fresh-users: FRESH disk (no USR LOG/PROF; run CONFIGURE's INIT in VICE)"
        fi
        "$ROOT/tools/assemble-d81.sh"
    fi
fi

# Verify disk image exists (built above, or expected pre-built with --no-build)
if [ ! -f "$DISK_IMAGE" ]; then
    echo "ERROR: Disk image not found: $DISK_IMAGE" >&2
    echo "Run without --no-build, or run 'make disk' first" >&2
    exit 1
fi

# Build VICE command line
VICE_ARGS=()

# Add fullscreen flag
if [ $FULLSCREEN -eq 1 ]; then
    VICE_ARGS+=("-fullscreen")
fi

# Add debugger flag
if [ $DEBUGGER -eq 1 ]; then
    VICE_ARGS+=("-debug")
fi

# Add config file if specified
if [ -n "$CONFIG" ]; then
    VICE_ARGS+=("-config" "$CONFIG")
fi

# Add pause flag
if [ $PAUSED -eq 1 ]; then
    VICE_ARGS+=("-pause")
fi

# Force unit 8 to the attached disk image.  A stale global VICE config can leave
# the IEC "virtual"/filesystem device enabled for unit 8 (e.g. FSDevice8Dir
# pointing at another project's directory); that shadows the BBS .d81 so BOOT's
# files aren't found and the machine drops to a DOS prompt.  +busdevice8 disables
# IEC device emulation for unit 8 so the true drive (the disk image) is used.
VICE_ARGS+=("+busdevice8")

# Add disk image
if [ $AUTOSTART -eq 1 ]; then
    VICE_ARGS+=("-autostart" "$DISK_IMAGE")
else
    VICE_ARGS+=("-8" "$DISK_IMAGE")
fi

echo "Launching VICE with TURBO64-${VERSION_COMPACT}.d81..."
echo "  Emulator: $VICE_CMD"
echo "  Disk:     $DISK_IMAGE"
if [ $USE_TCPSER -eq 1 ]; then
    echo "  tcpser:   port $TCPSER_PORT (virtual RS232 on $TCPSER_VPORT)"
fi
echo ""

TCPSER_PID=""

# Clean up any stale tcpser/VICE from a previous run before launching.
# An orphaned tcpser left holding the telnet port (with no emulator behind
# it) makes the BBS look "unreachable": callers connect but reach a dead
# bridge.  Kill anything bound to our ports / running our disk image first.
pkill -f "tcpser .*-p ${TCPSER_PORT}( |$)"  2>/dev/null || true
pkill -f "tcpser .*-v ${TCPSER_VPORT}( |$)" 2>/dev/null || true
pkill -f "${VICE_CMD} .*$(basename "$DISK_IMAGE")" 2>/dev/null || true
sleep 0.3

# Launch tcpser modem bridge
if [ $USE_TCPSER -eq 1 ]; then
    if ! command -v "$TCPSER_CMD" &>/dev/null; then
        echo "WARNING: tcpser not found ($TCPSER_CMD) — skipping modem bridge" >&2
        echo "Install tcpser (brew install tcpser) or set TCPSER_CMD" >&2
        USE_TCPSER=0
    else
        if [ $TCPSER_TRACE -eq 1 ]; then
            # -tsS traces serial in/out; capture both streams to the log so the
            # BBS<->modem byte exchange (RING/CONNECT/NO CARRIER, etc.) is visible.
            : > "$TCPSER_LOG"
            "$TCPSER_CMD" -v "$TCPSER_VPORT" -p "$TCPSER_PORT" -s 19200 -tsS >"$TCPSER_LOG" 2>&1 &
            TCPSER_PID=$!
            echo "tcpser launched (PID: $TCPSER_PID, telnet port: $TCPSER_PORT, trace -> $TCPSER_LOG)"
        else
            "$TCPSER_CMD" -v "$TCPSER_VPORT" -p "$TCPSER_PORT" -s 19200 &
            TCPSER_PID=$!
            echo "tcpser launched (PID: $TCPSER_PID, telnet port: $TCPSER_PORT)"
        fi
        # Brief pause to let tcpser bind its port before VICE connects
        sleep 0.5
    fi
fi

cleanup() {
    if [ -n "$TCPSER_PID" ] && kill -0 "$TCPSER_PID" 2>/dev/null; then
        echo "Stopping tcpser (PID: $TCPSER_PID)..."
        kill "$TCPSER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Add ACIA (SwiftLink) flags so VICE connects to tcpser.
# NOTE: -acia1base/-acia1mode/-acia1irq existed in VICE 3.8 but were removed
# in 3.10; the SwiftLink ACIA defaults to $DE00, which is what the BBS uses.
# Pin -rsdev1baud to match tcpser's -s AND the BBS's programmed ACIA rate.
# The BBS config BAUD_RATE=38400 maps to 6551 register bits 1111, which VICE
# emulates as 19200 (the 6551 has no 38400). The link MUST match that register
# rate or VICE's ACIA timing model intermittently drops the inbound CONNECT
# burst — so set tcpser -s and -rsdev1baud to 19200, not 9600.
if [ $USE_TCPSER -eq 1 ]; then
    VICE_ARGS+=(
        "-acia1"
        "-myaciadev"  "0"
        "-rsdev1"     "127.0.0.1:$TCPSER_VPORT"
        "-rsdev1baud" "19200"
    )
fi

# DS12C887 RTC cartridge at $D700 (avoids $DE00 ACIA conflict)
# -ds12c887rtcrunning: oscillator must be running or time registers stay at 0
# BBS probes $D500/$D600/$D700 in rtc_read_ds12c887()
VICE_ARGS+=(
    "-ds12c887rtc"
    "-ds12c887rtcbase" "0xD700"
    "-ds12c887rtcrunning"
)

# 16 MB REU (matches the Ultimate 64). Without an explicit size VICE falls back
# to the vicerc/default, which the BBS reports as a tiny/odd value.
VICE_ARGS+=(
    "-reu"
    "-reusize" "16384"
)

# JiffyDOS: swap the stock C64 KERNAL and 1581 drive ROM for the JiffyDOS
# patched versions (reference/jiffydos). The BBS boots a .d81, so drive 8 is a
# 1581 — both ROMs must match for the fast-loader handshake to engage.
if [ $JIFFYDOS -eq 1 ]; then
    if [ ! -f "$JIFFYDOS_KERNAL" ] || [ ! -f "$JIFFYDOS_1581" ]; then
        echo "ERROR: --jiffydos: ROM(s) not found in $JIFFYDOS_DIR" >&2
        [ -f "$JIFFYDOS_KERNAL" ] || echo "  missing: $(basename "$JIFFYDOS_KERNAL")" >&2
        [ -f "$JIFFYDOS_1581" ]   || echo "  missing: $(basename "$JIFFYDOS_1581")" >&2
        exit 1
    fi
    echo "  JiffyDOS: $(basename "$JIFFYDOS_KERNAL") + $(basename "$JIFFYDOS_1581")"
    VICE_ARGS+=(
        "-kernal"  "$JIFFYDOS_KERNAL"
        "-dos1581" "$JIFFYDOS_1581"
    )
fi

# Launch VICE
"$VICE_CMD" "${VICE_ARGS[@]}" &
VICE_PID=$!

echo "VICE launched (PID: $VICE_PID)"

# Guard: if VICE dies right away (bad option, crash), don't leave tcpser
# orphaned on the telnet port — that's the "connects but no BBS" trap.
sleep 1
if ! kill -0 "$VICE_PID" 2>/dev/null; then
    echo "ERROR: VICE exited immediately after launch (see output above)." >&2
    exit 1   # EXIT trap runs cleanup() and stops tcpser
fi

echo "Press Ctrl+C to exit"

# Wait for VICE to exit
wait $VICE_PID 2>/dev/null || true

echo "VICE closed."
