#!/usr/bin/env bash
# Builds Oscar64 from source into ./vendor/oscar64.
# Pinned to OSCAR64_REF for reproducible builds; override via env to try newer.
# Idempotent: if vendor/oscar64/bin/oscar64 exists, exits 0.
# To switch versions, remove vendor/oscar64/ and vendor/oscar64-src/ and re-run.
set -euo pipefail

OSCAR64_REF="${OSCAR64_REF:-895d04d45ebef1bc04cfe5dd5bdf253592505153}"

VENDOR="$(cd "$(dirname "$0")/.." && pwd)/vendor"
TARGET="$VENDOR/oscar64"

if [ -x "$TARGET/bin/oscar64" ]; then
    echo "oscar64 already installed at $TARGET/bin/oscar64"
    exit 0
fi

mkdir -p "$VENDOR"
cd "$VENDOR"

if [ ! -d oscar64-src ]; then
    # Blobless clone: any commit is checkout-able without full-history download.
    git clone --filter=blob:none https://github.com/drmortalwombat/oscar64.git oscar64-src
fi

cd oscar64-src
git fetch --quiet origin
git checkout --quiet "$OSCAR64_REF"

NPROC="$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
make -C make compiler -j"$NPROC"

mkdir -p "$TARGET/bin" "$TARGET/include" "$TARGET/lib"
cp bin/oscar64 "$TARGET/bin/"
cp -r include/* "$TARGET/include/" 2>/dev/null || true

echo "Installed oscar64 $OSCAR64_REF to $TARGET/bin/oscar64"
