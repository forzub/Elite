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

if SPLIT.get("stage") != "wave7D":
    raise AssertionError(f"physical modules: expected wave7D split metadata, got {SPLIT.get('stage')!r}")
if SPLIT.get("policy") != "move_without_redesign":
    raise AssertionError("physical modules: relocation policy drifted from move_without_redesign")

extracted = list(SPLIT.get("extracted_modules", []))
expected_extracted = [
    "shared", "transform_math", "semantics_transform",
    "source_maintenance", "source", "lods", "geometry", "surfaces",
    "shared_forms", "axis_mapping",
    "semantics_tree", "semantics_bindings", "semantics_workspace",
    "semantics_motion", "semantics_structural", "semantics_world_graph",
    "semantics_commands", "semantics_preview", "semantics_graph_viewport",
    "physics_stage", "physics_node", "physics_commands",
    "hit_volumes_list", "hit_volumes_inspector", "hit_volumes_commands", "hit_volumes_render_plan",
    "damage_stage", "damage_state_variants", "damage_node", "damage_render_selector", "damage_semantics",
    "final_assembly_validation", "final_assembly_build", "final_assembly_commands",
]
if extracted != expected_extracted:
    raise AssertionError(f"physical modules: unexpected wave7D extraction set {extracted}")

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
    incoming = any(module_name in (MODULES.get(other, {}).get("imports", {}) or {}) for other in extracted if other != module_name)
    if import_path not in HTML and not incoming:
        raise AssertionError(f"physical modules: {module_name} is neither imported by composition root nor another physical module")


# Static ESM graph sanity: every relative import resolves and every named import is
# actually exported by the target physical file. This catches a relocation that
# passes source-bundle analysis but would fail immediately in the browser loader.
physical_sources = [ROOT / MODULES[name]["physical_source"] for name in extracted]
source_by_path = {path.resolve(): path.read_text(encoding="utf-8") for path in physical_sources}
source_by_path[HTML_PATH.resolve()] = HTML
import_re = re.compile(r"import\s*\{([^}]*)\}\s*from\s*['\"]([^'\"]+)['\"]\s*;")
export_re = re.compile(r"export\s*\{([^}]*)\}\s*;")
for importer, source in source_by_path.items():
    base = importer.parent
    for match in import_re.finditer(source):
        names = {part.strip().split(" as ", 1)[0].strip() for part in match.group(1).split(",") if part.strip()}
        target_ref = match.group(2)
        if not target_ref.startswith("."):
            continue
        target = (base / target_ref).resolve()
        if not target.is_file():
            raise AssertionError(f"physical modules: unresolved relative import {target_ref!r} from {importer}")
        target_source = target.read_text(encoding="utf-8")
        exported: set[str] = set()
        for export_match in export_re.finditer(target_source):
            exported.update(part.strip().split(" as ", 1)[0].strip() for part in export_match.group(1).split(",") if part.strip())
        missing = names - exported
        if missing:
            raise AssertionError(
                f"physical modules: {importer.name} imports non-exported names from {target.name}: {sorted(missing)}"
            )

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
    "[PASS] Model Asset Editor physical split wave7D: "
    "shared/core stages + SEMANTICS + PHYSICS/HIT-VOLUMES/DAMAGE/FINAL-ASSEMBLY portable responsibilities extracted to real ES modules; "
    "546-function inventory preserved; runtime fallback + UI pack deployment wired"
)
