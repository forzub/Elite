#!/usr/bin/env python3
"""Architecture contract for the SOURCE active-LOD geometry browser (v0.10.62)."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(rel: str, token: str) -> None:
    data = text(rel)
    if token not in data:
        raise AssertionError(f"{rel}: missing {token!r}")


def function_body(data: str, name: str) -> str:
    needle = f"function {name}("
    start = data.find(needle)
    if start < 0:
        raise AssertionError(f"missing JS function {name}")
    brace = data.find("{", start)
    depth = 0
    quote = None
    escaped = False
    for i in range(brace, len(data)):
        ch = data[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            continue
        if ch in ("'", '"', '`'):
            quote = ch
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return data[start:i + 1]
    raise AssertionError(f"unterminated JS function {name}")


web_rel = "src/assets/webui/model_asset_editor.html"
web = text(web_rel)

# Folder authority, not a hand-maintained label, drives the catalog SOURCE tag.
for token in [
    "i.sourceAuthority==='folder'?'SOURCE':'RUNTIME'",
    "[${authority}]",
]:
    if token not in function_body(web, "renderCatalog"):
        raise AssertionError(f"catalog SOURCE/runtime authority label missing {token!r}")

# The SOURCE geometry group has its own compact LOD browser and visibility controls.
for token in [
    'id="geometryInventoryToolbar"',
    "geometryInventoryVisibleByLod:new Map()",
    "geometryInventorySelectedId:null",
    "function renderGeometryInventoryToolbar",
    "data-geometry-inventory-lod",
    "data-geometry-all-on",
    "data-geometry-all-off",
    "geometryInventoryMini",
    "geometryVisibilityCheck",
]:
    if token not in web:
        raise AssertionError(f"SOURCE geometry browser missing {token!r}")

# Visibility is per-LOD, editor-only viewport state. ALL removes the explicit filter;
# NONE installs an empty visible set. No persistence/backend mutation belongs here.
toolbar = function_body(web, "renderGeometryInventoryToolbar")
for token in [
    "geometryInventoryVisibleByLod.delete(Number(state.activeLod))",
    "geometryInventoryVisibleByLod.set(Number(state.activeLod),new Set())",
    "updateVisibility()",
]:
    if token not in toolbar:
        raise AssertionError(f"geometry toolbar visibility contract missing {token!r}")
if "send('save" in toolbar or "save_working" in toolbar:
    raise AssertionError("geometry visibility toolbar must remain viewport-only")

# Checkbox state gates actual render meshes, not semantic/group hierarchy.
visibility = function_body(web, "updateVisibility")
if "geometryInventoryNodeVisible(rn,lod)" not in visibility:
    raise AssertionError("geometry inventory visibility is not applied to viewport meshes")
for name in ("rebuildNormals", "rebuildEdgeOverlay"):
    if "geometryInventoryNodeVisible(node,lod)" not in function_body(web, name):
        raise AssertionError(f"{name} can leak an overlay for a hidden geometry")

# Clicking a geometry row resolves its representative RenderNode and uses the same
# exact render selection path as 3D picking. Selection UI is synchronized without
# making the checkbox itself steal the row click.
render = function_body(web, "renderGeometries")
for token in [
    "row.onclick=()=>selectGeometryInventoryDefinition(g)",
    "checkbox.onclick=e=>e.stopPropagation()",
    "checkbox.onchange=e=>",
    "existing=geometryInventoryVisibilitySet(li,false)",
    "set=new Set([id])",
    "state.geometryInventoryVisibleByLod.set(li,set)",
]:
    if token not in render:
        raise AssertionError(f"geometry row interaction missing {token!r}")
select = function_body(web, "selectGeometryInventoryDefinition")
for token in ["representativeRenderNodeForGeometry", "selectRenderNode(representative.index)"]:
    if token not in select:
        raise AssertionError(f"geometry list does not use canonical RenderNode selection: {token!r}")

# Switching from this local control follows the normal resident-LOD path and never
# mixes RELOAD LOD/SOURCE semantics into a view-only operation.
switch = function_body(web, "switchGeometryInventoryLod")
for token in ["send('load_lod'", "send('request_lod_payload'", "state.activeLod=index", "rebuildScene(true)"]:
    if token not in switch:
        raise AssertionError(f"geometry LOD selector missing {token!r}")
for forbidden in ("reload_lod", "reload_mesh_from_source", "reimport_asset", "save_working"):
    if forbidden in switch:
        raise AssertionError(f"geometry LOD view selector leaked mutating operation {forbidden!r}")

# Asset switch clears viewport-only source filters; they are never serialized.
if "state.geometryInventoryVisibleByLod.clear()" not in function_body(web, "acceptAssetState"):
    raise AssertionError("asset switch does not clear SOURCE geometry visibility state")

# The visibility gate is explicitly scoped to the SOURCE stage, so GEOMETRY /
# SURFACES / SEMANTICS and later stages retain their own viewport behavior.
if "state.wizardStage!==\'source\'" not in function_body(web, "geometryInventoryNodeVisible"):
    raise AssertionError("SOURCE geometry visibility filter leaks outside SOURCE stage")

# Contract and visible version fence.
require("tools/model_asset_editor/PATCH_CONTRACT.md", "SOURCE active-LOD mesh browser is viewport-only")
require("tools/model_asset_editor/PATCH_CONTRACT.md", "first checkbox interaction isolates that mesh")
require("tools/model_asset_editor/EditorVersion.h", 'ModelAssetEditorVersion = "0.10.64"')
print("[PASS] model asset editor v0.10.64 SOURCE geometry LOD browser / visibility / selection")
