#!/usr/bin/env bash
# build launcher script for TURBO/64 BBS development
#
# This script provides convenient shortcuts for common D8 development tasks:
#   - build build + run in VICE
#   - Deploy to Ultimate64
#   - Assemble disk images
#
# Usage: tools/build.sh [command] [options]
#
# Commands:
#   vice              Build and run in VICE emulator
#   u64               Build and deploy to Ultimate64
#   disk              Build disk image only
#   build             Build all binaries (no run/deploy)
#   clean             Clean build artifacts
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOL_DIR="$ROOT/tools"

# Color output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Show usage
show_usage() {
    cat << 'EOF'
TURBO/64 BBS build Launcher

Usage: tools/build.sh [command] [options]

Commands:
  vice              Build and run in VICE emulator
  u64               Build and deploy to Ultimate64
  disk              Assemble .d81 disk image only
  build             Build all binaries (boot + editor)
  clean             Remove build artifacts
  help              Show this message

User database options (for u64 and disk commands):
  --fetch-users     Fetch live USR LOG/PROF REL files from U64 into
                    data/ (updates users-seed.d81), then assemble
  --seed-users      Assemble using existing data/users-seed.d81
                    (previously fetched via --fetch-users or
                    tools/extract-users.sh)

VICE database options (for the vice command):
  (seeds data/users-seed.d81 by DEFAULT so the BBS boots straight to WFC)
  --fresh-users     Assemble a fresh disk with NO user DB, to exercise the
                    CONFIGURE INIT-required boot path
  --no-build        Boot the existing disk without rebuilding/reassembling
  --jiffydos        Boot with JiffyDOS ROMs (C64 kernal + 1581 drive)

Examples:
  tools/build.sh vice                     # Build, seed user DB, launch VICE (boots to WFC)
  tools/build.sh vice --trace             # ...and log the tcpser serial trace
  tools/build.sh vice --fresh-users       # Fresh disk (test the INIT-required path)
  tools/build.sh vice --jiffydos          # Boot with JiffyDOS ROMs
  tools/build.sh vice -f                  # Launch VICE in fullscreen
  tools/build.sh u64                      # Build and deploy (fresh disk, no users)
  tools/build.sh u64 -l sd               # Deploy to SD card
  tools/build.sh u64 --fetch-users       # Fetch live user DB from U64, build, deploy
  tools/build.sh u64 --seed-users        # Use bundled seed DB, build, deploy
  tools/build.sh disk                     # Assemble disk image only
  tools/build.sh build                    # Build binaries only

Environment:
  VICE_CMD          Override VICE command (default: x64sc)
  T64_SD_PATH        Override U64 deployment path

EOF
}

# Parse command
COMMAND="${1:-help}"
shift || true

case "$COMMAND" in
    vice)
        # deploy-vice.sh builds, assembles, and launches VICE. By default it
        # seeds USR LOG/USR PROF from data/users-seed.d81 (the same snapshot the
        # u64 --seed-users path uses) so the BBS boots straight to WFC instead of
        # halting on the missing INIT files. Flags pass through:
        #   --seed-users   (default) seed the user DB so it boots
        #   --fresh-users  assemble a fresh disk to exercise the CONFIGURE INIT path
        #   --no-build     boot the existing disk without rebuilding
        #   --trace, -f, -p, ...   forwarded to VICE / tcpser
        echo -e "${GREEN}Building + assembling + launching VICE...${NC}"
        "$TOOL_DIR/deploy-vice.sh" "$@"
        ;;
    
    u64)
        FETCH_USERS=0
        SEED_USERS=0
        DEPLOY_ARGS=()
        for arg in "$@"; do
            case "$arg" in
                --fetch-users) FETCH_USERS=1 ;;
                --seed-users)  SEED_USERS=1  ;;
                *) DEPLOY_ARGS+=("$arg") ;;
            esac
        done

        echo -e "${BLUE}Building D8 BBS...${NC}"
        cd "$ROOT" && make clean && make c64 && make editor
        echo ""

        if [ "$FETCH_USERS" -eq 0 ] && [ "$SEED_USERS" -eq 0 ]; then
            echo -e "${YELLOW}WARNING: No --fetch-users or --seed-users flag given.${NC}"
            echo -e "${YELLOW}         This will deploy a FRESH DISK — all existing BBS user data${NC}"
            echo -e "${YELLOW}         (USR LOG, USR PROF) on the U64 will be wiped.${NC}"
            echo ""
            read -r -p "Proceed? [y/N] " _confirm
            case "$_confirm" in
                [yY]|[yY][eE][sS]) ;;
                *) echo "Aborted."; exit 0 ;;
            esac
            echo ""
        fi

        if [ "$FETCH_USERS" -eq 1 ]; then
            echo -e "${BLUE}Fetching live user DB from U64...${NC}"
            "$TOOL_DIR/extract-users.sh"
            echo ""
        fi

        if [ "$SEED_USERS" -eq 1 ] && [ "$FETCH_USERS" -eq 0 ]; then
            echo -e "${YELLOW}WARNING: --seed-users will REPLACE all existing user data on the U64${NC}"
            echo -e "${YELLOW}         (USR LOG, USR PROF) with the seeded snapshot in data/users-seed.d81.${NC}"
            echo ""
            read -r -p "Proceed? [y/N] " _confirm
            case "$_confirm" in
                [yY]|[yY][eE][sS]) ;;
                *) echo "Aborted."; exit 0 ;;
            esac
            echo ""
        fi

        echo -e "${BLUE}Assembling disk image...${NC}"
        ASSEMBLE_ARGS=()
        if [ "$SEED_USERS" -eq 1 ] || [ "$FETCH_USERS" -eq 1 ]; then
            SEED_FILE="$ROOT/data/users-seed.d81"
            if [ -f "$SEED_FILE" ]; then
                ASSEMBLE_ARGS+=(--seed-users "$SEED_FILE")
            else
                echo -e "${YELLOW}WARNING: --seed-users requested but data/users-seed.d81 not found." >&2
                echo -e "         Run 'tools/build.sh u64 --fetch-users' first.${NC}" >&2
            fi
        fi
        "$TOOL_DIR/assemble-d81.sh" ${ASSEMBLE_ARGS[@]+"${ASSEMBLE_ARGS[@]}"}
        if [ "${#DEPLOY_ARGS[@]}" -gt 0 ]; then
            "$TOOL_DIR/deploy-u64.sh" "${DEPLOY_ARGS[@]}"
        else
            "$TOOL_DIR/deploy-u64.sh"
        fi
        ;;
    
    disk)
        FETCH_USERS=0
        SEED_USERS=0
        for arg in "$@"; do
            case "$arg" in
                --fetch-users) FETCH_USERS=1 ;;
                --seed-users)  SEED_USERS=1  ;;
            esac
        done

        if [ "$FETCH_USERS" -eq 1 ]; then
            echo -e "${BLUE}Fetching live user DB from U64...${NC}"
            "$TOOL_DIR/extract-users.sh"
            echo ""
        fi

        if [ "$SEED_USERS" -eq 1 ] && [ ! -f "$ROOT/data/users-seed.d81" ]; then
            echo -e "${YELLOW}WARNING: --seed-users requested but data/users-seed.d81 not found." >&2
            echo -e "         Run 'tools/build.sh u64 --fetch-users' first.${NC}" >&2
        fi

        echo -e "${BLUE}Assembling disk image...${NC}"
        ASSEMBLE_ARGS=()
        if [ "$SEED_USERS" -eq 1 ] || [ "$FETCH_USERS" -eq 1 ]; then
            SEED_FILE="$ROOT/data/users-seed.d81"
            [ -f "$SEED_FILE" ] && ASSEMBLE_ARGS+=(--seed-users "$SEED_FILE")
        fi
        "$TOOL_DIR/assemble-d81.sh" ${ASSEMBLE_ARGS[@]+"${ASSEMBLE_ARGS[@]}"}
        ;;
    
    build)
        echo -e "${BLUE}Building D8 BBS...${NC}"
        cd "$ROOT" && make clean && make c64 && make editor
        echo ""
        echo -e "${GREEN}Build complete!${NC}"
        echo "Binaries:"
        echo "  build/c64/BOOT-*.prg"
        echo "  build/c64/CONFIGURE-*.prg"
        ;;
    
    clean)
        echo -e "${YELLOW}Cleaning build artifacts...${NC}"
        cd "$ROOT" && make clean
        echo -e "${GREEN}Clean complete!${NC}"
        ;;
    
    help|--help|-h)
        show_usage
        ;;
    
    *)
        echo "Unknown command: $COMMAND" >&2
        echo "Run 'tools/build.sh help' for usage" >&2
        exit 1
        ;;
esac
