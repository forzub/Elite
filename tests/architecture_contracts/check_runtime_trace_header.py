#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src/core/RuntimeTrace.h"
INCLUDE = '#include "src/core/RuntimeTrace.h"'


def fail(message: str) -> None:
    print(f"[FAIL] runtime trace header contract: {message}", file=sys.stderr)
    raise SystemExit(1)


if not HEADER.is_file():
    fail("src/core/RuntimeTrace.h is required by runtime log hygiene but is missing")

header_text = HEADER.read_text(encoding="utf-8")
for needle in ("runtimeTraceEnabled", "ELITE_TRACE_RUNTIME"):
    if needle not in header_text:
        fail(f"RuntimeTrace.h is missing required token: {needle}")

users = []
for source in (ROOT / "src").rglob("*"):
    if not source.is_file() or source.suffix.lower() not in {".h", ".hpp", ".cpp", ".cc", ".cxx"}:
        continue
    if INCLUDE in source.read_text(encoding="utf-8", errors="ignore"):
        users.append(source.relative_to(ROOT).as_posix())

if not users:
    fail("RuntimeTrace.h exists but no runtime source includes it")

print(f"[PASS] runtime trace header is packaged and referenced by {len(users)} runtime sources")
