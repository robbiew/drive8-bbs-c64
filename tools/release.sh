#!/usr/bin/env bash
# Full release pipeline for TURBO/64 BBS: build, tag, push, publish GitHub release
#
# Usage: tools/release.sh              # full release
#        tools/release.sh --dry-run    # build artifacts, show plan, no tag/push/publish
#        tools/release.sh --skip-tag   # build + publish to existing tag

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

DRY_RUN=0
SKIP_TAG=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run)  DRY_RUN=1; shift ;;
        --skip-tag) SKIP_TAG=1; shift ;;
        -h|--help)
            sed -n '2,7p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Usage: tools/release.sh [--dry-run|--skip-tag]" >&2
            exit 1
            ;;
    esac
done

# --- Prerequisites ---

command -v gh >/dev/null 2>&1 || {
    echo -e "${RED}ERROR: gh CLI is not installed.${NC}" >&2
    echo "Install from: https://cli.github.com/" >&2
    exit 1
}

gh auth status >/dev/null 2>&1 || {
    echo -e "${RED}ERROR: gh CLI is not authenticated.${NC}" >&2
    echo "Run: gh auth login" >&2
    exit 1
}

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo -e "${RED}ERROR: Working tree is dirty. Commit or stash changes first.${NC}" >&2
    git status --short >&2
    exit 1
fi

# --- Version ---

VERSION="$(grep 'BBS_RELEASE_VERSION_COMPACT' "$ROOT/include/bbs/version.h" | cut -d'"' -f2)"
if [ -z "$VERSION" ]; then
    echo -e "${RED}ERROR: Could not read version from include/bbs/version.h${NC}" >&2
    exit 1
fi
TAG="v$VERSION"

echo -e "${BLUE}TURBO/64 BBS release pipeline${NC}"
echo -e "  Version: ${GREEN}$VERSION${NC}"
echo -e "  Tag:     ${GREEN}$TAG${NC}"
echo ""

# --- Tag check ---

if [ "$SKIP_TAG" -eq 0 ]; then
    if git rev-parse "$TAG" >/dev/null 2>&1; then
        echo -e "${RED}ERROR: Tag $TAG already exists.${NC}" >&2
        echo "Use --skip-tag to publish to an existing tag, or bump the version." >&2
        exit 1
    fi
fi

# --- Build ---

echo -e "${BLUE}Cleaning and building...${NC}"
make -C "$ROOT" clean && make -C "$ROOT" all && make -C "$ROOT" disk
echo ""

# --- Verify build outputs ---

BOOT_PRG="$ROOT/build/c64/BOOT-${VERSION}.prg"
CONFIGURE_PRG="$ROOT/build/c64/CONFIGURE-${VERSION}.prg"
DISK_IMG="$ROOT/build/c64/TURBO64-${VERSION}.d81"
BLANK_DISK="$ROOT/data/blank-disk.d81"

for f in "$BOOT_PRG" "$CONFIGURE_PRG" "$DISK_IMG" "$BLANK_DISK"; do
    if [ ! -f "$f" ]; then
        echo -e "${RED}ERROR: Missing build output: $f${NC}" >&2
        exit 1
    fi
done

# --- Stage release assets ---

RELEASE_DIR="$ROOT/build/release"
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

cp "$DISK_IMG"      "$RELEASE_DIR/TURBO64-${VERSION}.d81"
cp "$BOOT_PRG"      "$RELEASE_DIR/BOOT-${VERSION}.prg"
cp "$CONFIGURE_PRG" "$RELEASE_DIR/CONFIGURE-${VERSION}.prg"
cp "$BLANK_DISK"    "$RELEASE_DIR/BOARDS-${VERSION}.d81"

sed "s/__VERSION__/$VERSION/g" "$ROOT/data/release/file_id.diz.tmpl" > "$RELEASE_DIR/FILE_ID.DIZ"
sed "s/__VERSION__/$VERSION/g" "$ROOT/data/release/readme.txt.tmpl" > "$RELEASE_DIR/README.txt"

echo -e "${GREEN}Release assets staged:${NC}"
ls -1 "$RELEASE_DIR"
echo ""

# --- Dry run exit ---

if [ "$DRY_RUN" -eq 1 ]; then
    echo -e "${YELLOW}DRY RUN — no tag, push, or publish will be performed.${NC}"
    echo ""
    echo "Planned steps:"
    echo "  1. git tag -a \"$TAG\" -m \"TURBO/64 BBS v$VERSION\""
    echo "  2. git push origin \"$TAG\""
    echo "  3. gh release create \"$TAG\" \\"
    echo "       build/release/TURBO64-${VERSION}.d81 \\"
    echo "       build/release/BOOT-${VERSION}.prg \\"
    echo "       build/release/CONFIGURE-${VERSION}.prg \\"
    echo "       build/release/BOARDS-${VERSION}.d81 \\"
    echo "       build/release/FILE_ID.DIZ \\"
    echo "       build/release/README.txt \\"
    echo "       --title \"TURBO/64 BBS v$VERSION\" \\"
    echo "       --notes \"<changelog>\""
    echo ""
    echo -e "${GREEN}Dry run complete.${NC}"
    exit 0
fi

# --- Tag and push ---

if [ "$SKIP_TAG" -eq 0 ]; then
    echo -e "${BLUE}Creating tag $TAG...${NC}"
    git tag -a "$TAG" -m "TURBO/64 BBS v$VERSION"
    echo -e "${BLUE}Pushing tag $TAG...${NC}"
    git push origin "$TAG"
    echo ""
fi

# --- Changelog ---

PREV_TAG="$(git tag --sort=-version:refname 2>/dev/null | head -2 | tail -1)" || true
if [ -n "$PREV_TAG" ] && [ "$PREV_TAG" != "$TAG" ]; then
    CHANGELOG="$(git log "$PREV_TAG..HEAD" --oneline)"
else
    CHANGELOG="$(git log --oneline)"
fi

# --- GitHub release ---

echo -e "${BLUE}Creating GitHub release $TAG...${NC}"
gh release create "$TAG" \
    "$RELEASE_DIR/TURBO64-${VERSION}.d81" \
    "$RELEASE_DIR/BOOT-${VERSION}.prg" \
    "$RELEASE_DIR/CONFIGURE-${VERSION}.prg" \
    "$RELEASE_DIR/BOARDS-${VERSION}.d81" \
    "$RELEASE_DIR/FILE_ID.DIZ" \
    "$RELEASE_DIR/README.txt" \
    --title "TURBO/64 BBS v$VERSION" \
    --notes "$CHANGELOG"

echo ""
echo -e "${GREEN}Release v$VERSION published!${NC}"