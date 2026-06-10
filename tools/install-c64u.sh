#!/usr/bin/env bash
# Downloads the c64u CLI binary from GitHub Releases into ./vendor/c64u/bin/c64u.
# Idempotent: if vendor/c64u/bin/c64u exists and is executable, exits 0.
set -euo pipefail

VENDOR="$(cd "$(dirname "$0")/.." && pwd)/vendor"
TARGET="$VENDOR/c64u"
BIN="$TARGET/bin/c64u"

if [ -x "$BIN" ]; then
    echo "c64u already installed at $BIN"
    exit 0
fi

OS="$(uname -s)"
ARCH="$(uname -m)"

case "${OS}_${ARCH}" in
    Darwin_arm64)   ASSET="c64u_Darwin_arm64.tar.gz" ;;
    Darwin_x86_64)  ASSET="c64u_Darwin_x86_64.tar.gz" ;;
    Linux_x86_64)   ASSET="c64u_Linux_x86_64.tar.gz" ;;
    Linux_aarch64)  ASSET="c64u_Linux_arm64.tar.gz" ;;
    Linux_arm64)    ASSET="c64u_Linux_arm64.tar.gz" ;;
    *)
        echo "Unsupported platform: ${OS} ${ARCH}" >&2
        exit 1
        ;;
esac

URL="https://github.com/cybersorcerer/c64u/releases/latest/download/${ASSET}"

mkdir -p "$TARGET/bin"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading ${ASSET}..."
curl -fsSL "$URL" | tar xz -C "$TMP"

cp "$TMP/c64u" "$BIN"
chmod +x "$BIN"

echo "Installed c64u to $BIN"
