"""Shared Model Asset Editor version guard for the architecture gates.

Gates record the editor version that introduced their contract as a MINIMUM.
An exact pin turned every gate red on each later deliberate version bump, so
the gates only require that the editor has not regressed below that version.
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VERSION_HEADER = ROOT / "tools/model_asset_editor/EditorVersion.h"


def editor_version() -> tuple[int, ...]:
    text = VERSION_HEADER.read_text(encoding="utf-8", errors="replace")
    match = re.search(r'ModelAssetEditorVersion\s*=\s*"([0-9.]+)"', text)
    if not match:
        raise AssertionError(f"{VERSION_HEADER}: ModelAssetEditorVersion not found")
    return tuple(int(part) for part in match.group(1).split("."))


def editor_version_at_least(minimum: str) -> bool:
    return editor_version() >= tuple(int(part) for part in minimum.split("."))


def require_editor_version_at_least(minimum: str) -> None:
    if not editor_version_at_least(minimum):
        current = ".".join(str(part) for part in editor_version())
        raise AssertionError(f"editor version {current} is older than the contract minimum {minimum}")
