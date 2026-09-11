#!/usr/bin/env python3
"""Architecture contract for the SOURCE active-LOD geometry browser (v0.10.62)."""
from pathlib import Path

from model_asset_editor_source_bundle import load_source_bundle

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
web = load_source_bundle(ROOT)

# v0.10.67: actual linked-folder availability, not RuntimeAssembly bootstrap, drives
# the user-facing catalog SOURCE-presence icon.
catalog = function_body(web, "renderCatalog")
if "sourceIcon=i.sourceAvailable?'📁':'📄'" not in catalog:
    raise AssertionError("catalog SOURCE presence icon is not driven by sourceAvailable")
if "[SOURCE]" in catalog or "[RUNTIME]" in catalog:
    raise AssertionError("catalog still exposes SOURCE/RUNTIME authority labels")

# The SOURCE geometry group has its own compact LOD browser and visibility controls.
for token in [
    'id="geometryInventoryToolbar"',
    "geometryInventoryVisibleByLod:{get:()=>editorViewState.geometryInventoryVisibility}",
    "geometryInventorySelectedId:{get:()=>editorViewState.selectedMeshId",
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

# Checkbox state projects into the authoritative EditorViewState; physical mesh
# visibility reads that one state rather than independently ANDing the SOURCE map.
visibility = function_body(web, "updateVisibility")
if "editorViewState.renderNodeVisible(state.activeLod,i,rn)" not in visibility:
    raise AssertionError("SOURCE geometry visibility no longer reaches authoritative viewport state")
if "geometryInventoryNodeVisible(rn,lod)" in visibility:
    raise AssertionError("SOURCE geometry map returned as an independent viewport visibility owner")
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
    "set=new Set([effectiveId])",
    "state.geometryInventoryVisibleByLod.set(li,set)",
]:
    if token not in render:
        raise AssertionError(f"geometry row interaction missing {token!r}")
select = function_body(web, "selectGeometryInventoryDefinition")
for token in ["representativeRenderNodeForGeometry", "aliasRepresentativeRenderNode", "selectRenderNode(representative.index)"]:
    if token not in select:
        raise AssertionError(f"geometry list does not use canonical/instance RenderNode selection: {token!r}")

# Switching from this local control follows the normal resident-LOD path and never
# mixes RELOAD LOD/SOURCE semantics into a view-only operation.
switch = function_body(web, "switchGeometryInventoryLod")
for token in ["send('load_lod'", "send('request_lod_payload'", "state.activeLod=index", "rebuildScene(true)"]:
    if token not in switch:
        raise AssertionError(f"geometry LOD selector missing {token!r}")
for forbidden in ("reload_lod", "reload_mesh_from_source", "reimport_asset", "save_working"):
    if forbidden in switch:
        raise AssertionError(f"geometry LOD view selector leaked mutating operation {forbidden!r}")

# A different asset resets the single editor view state. Visibility is not serialized.
if "editorViewState.resetForAsset()" not in function_body(web, "acceptAssetState"):
    raise AssertionError("asset switch does not reset authoritative EditorViewState")

# The accepted SOURCE helper remains SOURCE-scoped for its own UI projection.
# Physical visibility persists across tabs through EditorViewState/updateVisibility.
if "state.wizardStage!==\'source\'" not in function_body(web, "geometryInventoryNodeVisible"):
    raise AssertionError("accepted SOURCE projection helper lost its stage scope")

# Contract and visible version fence.
require("tools/model_asset_editor/PATCH_CONTRACT.md", "SOURCE active-LOD mesh browser is viewport-only")
require("tools/model_asset_editor/PATCH_CONTRACT.md", "first checkbox interaction isolates that mesh")
require("tools/model_asset_editor/EditorVersion.h", 'ModelAssetEditorVersion = "0.10.67"')
print("[PASS] model asset editor v0.10.67 SOURCE geometry LOD browser / visibility / selection")
