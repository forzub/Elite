#!/usr/bin/env python3
"""Physical-module extraction contract for Model Asset Editor wave7+.

Logical ownership stays authoritative. Once an ownership module gets a
`physical_source`, its owned portable implementation must live there rather than in
model_asset_editor.html, the composition root must import the declared public
surface, and editor deployment must package/copy the physical module tree.
"""
from __future__ import annotations

from pathlib import Path
import importlib.util
import json
import re

import check_model_asset_function_purity as purity
from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]
HTML_PATH = ROOT / "src/assets/webui/model_asset_editor.html"
HTML = HTML_PATH.read_text(encoding="utf-8")
OWNERSHIP = json.loads((ROOT / "tools/model_asset_editor/MODULE_OWNERSHIP_CONTRACT.json").read_text(encoding="utf-8"))
MODULES = OWNERSHIP.get("modules", {})
SPLIT = OWNERSHIP.get("physical_split", {})

if SPLIT.get("stage") != "wave7B":
    raise AssertionError(f"physical modules: expected wave7B split metadata, got {SPLIT.get('stage')!r}")
if SPLIT.get("policy") != "move_without_redesign":
    raise AssertionError("physical modules: relocation policy drifted from move_without_redesign")

extracted = list(SPLIT.get("extracted_modules", []))
expected_extracted = [
    "shared", "transform_math", "semantics_transform",
    "source_maintenance", "source", "lods", "geometry", "surfaces",
]
if extracted != expected_extracted:
    raise AssertionError(f"physical modules: unexpected wave7B extraction set {extracted}")

for module_name in extracted:
    spec = MODULES.get(module_name) or {}
    rel = spec.get("physical_source")
    if not rel:
        raise AssertionError(f"physical modules: {module_name} has no physical_source")
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"physical modules: missing {rel}")
    src = path.read_text(encoding="utf-8")
    owned = list(spec.get("core", [])) + list(spec.get("presentation", []))
    for name in owned:
        marker = f"function {name}("
        if marker not in src:
            raise AssertionError(f"physical modules: {module_name}.{name} missing from {rel}")
        if marker in HTML:
            raise AssertionError(f"physical modules: {module_name}.{name} still implemented inline in HTML")
    exports = list(spec.get("exports", []))
    import_path = "./" + str(Path(rel).relative_to("src/assets/webui")).replace("\\", "/")
    if import_path not in HTML:
        raise AssertionError(f"physical modules: composition root does not import {import_path}")
    for name in exports:
        if not re.search(rf"\b{re.escape(name)}\b", HTML):
            raise AssertionError(f"physical modules: imported API {module_name}.{name} is not wired in composition root")

# The architecture inventory remains invariant across pure relocation.
bundle = load_source_bundle(ROOT)
names = purity._named_function_names(bundle)
if len(names) != 546 or len(set(names)) != 546:
    raise AssertionError(f"physical modules: named-function inventory drifted: total={len(names)} unique={len(set(names))}")

# Runtime fallback and resource pack must both carry the extracted module tree.
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
for token in (
    'file(GLOB_RECURSE ELITE_MODEL_ASSET_EDITOR_MODULE_FILES CONFIGURE_DEPENDS',
    'webui/model_asset_editor/*.js',
    '${CMAKE_COMMAND} -E copy_directory',
    '--include model_asset_editor/*.js',
):
    if token not in cmake:
        raise AssertionError(f"physical modules: editor deployment missing {token!r}")

# Prove the pack include glob actually selects every physical JS source.
spec = importlib.util.spec_from_file_location("build_ui_pack", ROOT / "tools/build_ui_pack.py")
assert spec and spec.loader
build_ui_pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_ui_pack)
items = build_ui_pack.collect(
    ROOT / "src/assets/webui",
    ROOT / "__missing_fonts__",
    ROOT / "__missing_licenses__",
    includes=[
        "model_asset_editor.html",
        "elite_ui.css",
        "elite_ui.js",
        "model_asset_editor/*.js",
        "vendor/three/three.module.js",
        "vendor/three/examples/jsm/controls/OrbitControls.js",
    ],
)
packed = {resource for resource, _ in items}
for module_name in extracted:
    rel = MODULES[module_name]["physical_source"]
    resource = "/" + str(Path(rel).relative_to("src/assets/webui")).replace("\\", "/")
    if resource not in packed:
        raise AssertionError(f"physical modules: UI pack glob omitted {resource}")

print(
    "[PASS] Model Asset Editor physical split wave7B: "
    "shared/transform-math/semantics-transform + SOURCE/LODS/GEOMETRY/SURFACES portable cores extracted to real ES modules; "
    "546-function inventory preserved; runtime fallback + UI pack deployment wired"
)
