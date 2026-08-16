#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
FORBIDDEN = (
    "D:/__elite/work",
    "D:\\\\__elite\\\\work",
)

violations = []
for path in SRC.rglob("*"):
    if not path.is_file() or path.suffix.lower() not in {
        ".cpp", ".h", ".hpp", ".c", ".cc", ".json", ".html", ".js"
    }:
        continue
    text = path.read_text(encoding="utf-8", errors="replace")
    for token in FORBIDDEN:
        if token in text:
            violations.append(f"{path.relative_to(ROOT)}: {token}")

if violations:
    print("[FAIL] hardcoded developer checkout paths remain:", file=sys.stderr)
    for item in violations:
        print(f"  {item}", file=sys.stderr)
    raise SystemExit(1)

print("[PASS] runtime source is independent of the developer checkout path")
