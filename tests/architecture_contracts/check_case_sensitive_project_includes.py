#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = {".h", ".hpp", ".hh", ".c", ".cc", ".cpp", ".cxx"}
SCAN_ROOTS = [ROOT / "src", ROOT / "tests", ROOT / "glad"]


def fail(message: str) -> None:
    print(f"[FAIL] case-sensitive project includes: {message}", file=sys.stderr)
    raise SystemExit(1)


project_files = []
for scan_root in SCAN_ROOTS:
    if scan_root.exists():
        project_files.extend(p for p in scan_root.rglob("*") if p.is_file())

files = {
    p.resolve().relative_to(ROOT.resolve()).as_posix(): p
    for p in project_files
}
files_lower = {name.lower(): name for name in files}
issues = []

for source in project_files:
    if source.suffix.lower() not in SOURCE_SUFFIXES:
        continue

    text = source.read_text(encoding="utf-8", errors="ignore")
    for match in re.finditer(r'^\s*#\s*include\s*"([^\"]+)"', text, re.MULTILINE):
        include = match.group(1).replace("\\", "/")
        candidates = []
        for base in (source.parent, ROOT, ROOT / "src"):
            candidate = (base / include).resolve()
            try:
                rel = candidate.relative_to(ROOT.resolve()).as_posix()
            except ValueError:
                continue
            candidates.append(rel)

        if any(candidate in files for candidate in candidates):
            continue

        case_match = next(
            (files_lower[candidate.lower()]
             for candidate in candidates
             if candidate.lower() in files_lower),
            None,
        )
        if case_match is not None:
            issues.append((source.relative_to(ROOT).as_posix(), include, case_match))

if issues:
    details = "; ".join(
        f"{source}: {include} -> {actual}"
        for source, include, actual in sorted(set(issues))
    )
    fail(details)

print("[PASS] project-local include casing is Linux-safe")
