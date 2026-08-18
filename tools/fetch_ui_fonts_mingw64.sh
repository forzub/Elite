#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# The user's repository already owns its .gitignore. Do not mutate or replace it
# from tooling. Fetched font binaries are machine-local cache artifacts, so keep
# them out of `git status` through this checkout's private exclude file instead.
if [[ -d "$ROOT_DIR/.git" ]]; then
  EXCLUDE_FILE="$ROOT_DIR/.git/info/exclude"
  mkdir -p "$(dirname "$EXCLUDE_FILE")"
  touch "$EXCLUDE_FILE"
  for pattern in     '/third_party/fonts/noto/*.ttf'     '/third_party/fonts/noto/*.otf'     '/third_party/fonts/noto/*.woff2'; do
    grep -Fqx -- "$pattern" "$EXCLUDE_FILE" || printf '%s\n' "$pattern" >> "$EXCLUDE_FILE"
  done
fi

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$ROOT_DIR/tools/fetch_ui_fonts.ps1")"
