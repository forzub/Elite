#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tests"
SOURCE_EXTENSIONS = r"(?:cpp|cxx|cc|c|hpp|h)"


def fail(messages: list[str]) -> None:
    print("Test-suite source integrity check failed:", file=sys.stderr)
    for message in messages:
        print(f"  - {message}", file=sys.stderr)
    raise SystemExit(1)


def cmake_source_tokens(body: str) -> set[str]:
    tokens: set[str] = set()

    # Repository-root sources are explicit and safe to resolve.
    root_pattern = re.compile(
        rf"(\$\{{ELITE_SOURCE_ROOT\}}/"
        rf"[A-Za-z0-9_./+\-]+\.{SOURCE_EXTENSIONS})"
        rf"(?![A-Za-z0-9_.])"
    )
    tokens.update(match.group(1) for match in root_pattern.finditer(body))

    # Test-local sources are normally listed as bare filenames. Restrict this
    # pattern to names without '/' so external include probes such as
    # glm/gtx/norm.hpp or URLs are not mistaken for repository sources.
    local_pattern = re.compile(
        rf"(?<![A-Za-z0-9_./\-])"
        rf"([A-Za-z0-9_+\-]+\.{SOURCE_EXTENSIONS})"
        rf"(?![A-Za-z0-9_.])"
    )
    tokens.update(match.group(1) for match in local_pattern.finditer(body))

    return tokens


def resolve_cmake_source(cmake: Path, token: str) -> Path:
    prefix = "${ELITE_SOURCE_ROOT}/"
    if token.startswith(prefix):
        return ROOT / token[len(prefix):]
    return cmake.parent / token


errors: list[str] = []

for cmake in sorted(TESTS.glob("*/CMakeLists.txt")):
    body = cmake.read_text(encoding="utf-8", errors="replace")
    for token in sorted(cmake_source_tokens(body)):
        resolved = resolve_cmake_source(cmake, token)
        if not resolved.is_file():
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
        r"(tests/[A-Za-z0-9_./-]+\.(?:sh|py))",
        body,
    ):
        rel = match.group(1)
        if not (ROOT / rel).is_file():
            errors.append(f"{runner_rel} invokes missing script {rel}")

if errors:
    fail(errors)

print("[PASS] all registered test CMake sources and runner scripts exist")
