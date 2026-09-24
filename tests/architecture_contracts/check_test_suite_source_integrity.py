#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests"


def fail(messages: list[str]) -> None:
    print("Test-suite source integrity check failed:", file=sys.stderr)
    for message in messages:
        print(f"  - {message}", file=sys.stderr)
    raise SystemExit(1)


def resolve_cmake_source(cmake: Path, token: str) -> Path | None:
    token = token.strip()
    prefix = "${ELITE_SOURCE_ROOT}/"
    if token.startswith(prefix):
        return ROOT / token[len(prefix):]
    if "${" in token:
        return None
    path = Path(token)
    if path.is_absolute():
        return path
    return cmake.parent / path


def cmake_source_tokens(body: str) -> set[str]:
    pattern = re.compile(
        r'(?<![A-Za-z0-9_./-])'
        r'((?:\$\{ELITE_SOURCE_ROOT\}/)?'
        r'[A-Za-z0-9_./{}$+-]+\.(?:c|cc|cpp|cxx|h|hpp))'
    )
    return {match.group(1) for match in pattern.finditer(body)}


errors: list[str] = []

for cmake in sorted(TESTS.glob("*/CMakeLists.txt")):
    body = cmake.read_text(encoding="utf-8", errors="replace")
    for token in sorted(cmake_source_tokens(body)):
        resolved = resolve_cmake_source(cmake, token)
        if resolved is not None and not resolved.is_file():
            errors.append(
                f"{cmake.relative_to(ROOT)} references missing source {token}"
            )

for runner_rel in (
    "tests/run_all_mingw64.sh",
    "tests/architecture_contracts/run_mingw64.sh",
):
    runner = ROOT / runner_rel
    body = runner.read_text(encoding="utf-8", errors="replace")
    for match in re.finditer(
        r'(tests/[A-Za-z0-9_./-]+\.(?:sh|py))',
        body,
    ):
        rel = match.group(1)
        if not (ROOT / rel).is_file():
            errors.append(f"{runner_rel} invokes missing script {rel}")

if errors:
    fail(errors)

print("[PASS] all registered test CMake sources and runner scripts exist")
