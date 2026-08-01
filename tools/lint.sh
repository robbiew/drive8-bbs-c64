#!/usr/bin/env bash
# Run cppcheck across BBS source. Treats certain styles as errors.
set -euo pipefail

if ! command -v cppcheck >/dev/null 2>&1; then
    echo "cppcheck not installed (apt install cppcheck)"
    exit 1
fi

cppcheck \
    --std=c99 \
    --enable=warning,style,performance,portability \
    --inline-suppr \
    --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    -I include \
    -I src/term \
    -I src \
    -I src-editor \
    -I src-diag \
    src src-editor src-diag include
