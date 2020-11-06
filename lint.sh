#!/bin/sh

set -ex

ROOT="$(dirname "$0")"
PATH="$PATH:$HOME/.local/bin"

error () { echo "ERROR: $@" >&2; exit 1; }

cppcheck --error-exitcode=1 \
    --enable=warning,performance,portability,unusedFunction,missingInclude \
    --suppress=unusedFunction:"$ROOT/src/macros.h" \
    -Ug_auto -Ug_autofree -Ug_autoptr \
    "$ROOT"/src/*.[ch]

cpplint --counting=detailed --recursive \
    --filter=-build/endif_comment \
    "$ROOT"/src

grep -RF $'\t' "$ROOT"/src &&
    error 'Tabs found. Use spaces.' || echo 'Whitespace OK'

grep -RP '[^[:cntrl:][:print:]]' "$ROOT"/src &&
    error 'No emojis.' || echo 'Emojis OK'
