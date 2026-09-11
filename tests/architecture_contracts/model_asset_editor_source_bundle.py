#!/usr/bin/env python3
"""Source aggregation helpers for the physically split Model Asset Editor WebUI.

Architecture contracts reason about the editor as one JS program even while named
functions move from the composition-root HTML into real ES modules. Runtime imports
remain authoritative; this helper is test-only source discovery.
"""
from __future__ import annotations

from pathlib import Path


def source_paths(root: Path) -> list[Path]:
    html = root / "src/assets/webui/model_asset_editor.html"
    module_root = root / "src/assets/webui/model_asset_editor"
    modules = sorted(path for path in module_root.rglob("*.js") if path.is_file()) if module_root.is_dir() else []
    return [html, *modules]


def load_source_bundle(root: Path) -> str:
    parts: list[str] = []
    for path in source_paths(root):
        relative = path.relative_to(root).as_posix()
        parts.append(f"\n/* === MODEL_ASSET_EDITOR_SOURCE: {relative} === */\n")
        parts.append(path.read_text(encoding="utf-8", errors="replace"))
    return "".join(parts)
