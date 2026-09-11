#!/usr/bin/env python3
"""v0.10.64: GEOMETRY tab is a per-mesh certified workspace with viewport filtering."""
from pathlib import Path

from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]


def text(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def js_function(data: str, name: str) -> str:
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


def cpp_function(data: str, signature: str) -> str:
    start = data.find(signature)
    if start < 0:
        raise AssertionError(f"missing C++ function {signature}")
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
        if ch in ("'", '"'):
            quote = ch
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return data[start:i + 1]
    raise AssertionError(f"unterminated C++ function {signature}")


web = load_source_bundle(ROOT)
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
version = text("tools/model_asset_editor/EditorVersion.h")
contract = text("tools/model_asset_editor/PATCH_CONTRACT.md")

geometry_start = web.index("if(stage==='geometry'){")
geometry_end = web.index("if(stage==='surfaces')", geometry_start)
geometry_branch = web[geometry_start:geometry_end]
geometry_block = geometry_branch + js_function(web, "wizardGeometryStageModel") + js_function(web, "wizardGeometryStageHtml") + js_function(web, "renderWizardGeometryStage")

# Retire the old maintenance workset selector only from GEOMETRY. Shared helpers
# are allowed to remain because SOURCE/SURFACES are separate accepted surfaces.
for forbidden in (
    "geometryScopeSelector",
    "data-geometry-scope",
    "bindGeometryScopeControls",
    "RECENTLY LOADED / CHANGED",
):
    if forbidden in geometry_block:
        raise AssertionError(f"old GEOMETRY workset UI survived: {forbidden!r}")

for required in (
    "geometryStageTable",
    "geometryCompareAllBtn",
    "geometryCompareNoneBtn",
    "showAll:'ПОКАЗАТЬ ВСЕ'",
    "hideAll:'СПРЯТАТЬ ВСЕ'",
    ">● ${text.showAll}</button>",
    ">○ ${text.hideAll}</button>",
    "wizardStageCheckControls('geometry')",
    "wizardGeometryScanBtn",
    "wizardGeometryConsolidateBtn",
    "wizardGeometryCleanBtn",
):
    if required not in geometry_block:
        raise AssertionError(f"GEOMETRY workspace missing {required!r}")

# Table is taller and stage color remains the primary row state even when the
# duplicate-comparison overlay labels a row REF/MATCH/DIFFERENT/INSTANCE.
for token in (
    ".geometryWorkspace .geometryStageTable{max-height:min(66vh,720px);min-height:390px}",
    ".geometryTechButton{min-width:0;height:25px",
    ".geometryWorkspace .compareRow.meshStagePassed",
    ".geometryWorkspace .compareRow.meshValidationPending",
    ".geometryWorkspace .compareRow.visibilityOff",
):
    if token not in web:
        raise AssertionError(f"GEOMETRY table/color contract missing {token!r}")

candidates = js_function(web, "renderGeometryCandidatesContents")
for token in (
    "meshStageVisualClass(item.geometry,'geometry')",
    "geometryStageNodeVisible(i)",
    "row.dataset.geometryRenderNode=String(i)",
    "compareShowToggle",
    "setGeometryStageNodeVisible(i,checkbox.checked)",
    "selectRenderNode(i,{scrollGeometry:false})",
):
    if token not in candidates:
        raise AssertionError(f"GEOMETRY table behavior missing {token!r}")

# Visibility is viewport-local and independent per LOD. The first checkbox
# interaction from default-all isolates the clicked mesh, matching SOURCE/LOD.
for token in (
    "geometryStageVisibleByLod:{get:()=>editorViewState.geometryStageVisibility}",
    "function geometryStageVisibilitySet(",
    "function geometryStageNodeVisible(",
    "function setGeometryStageNodeVisible(",
    "if(existing===null){set=new Set([i]);}",
    "function showAllGeometryStageMeshes(",
    "function hideAllGeometryStageMeshes(",
    "editorViewVisible=editorViewState.renderNodeVisible(state.activeLod,i,rn)",
):
    if token not in web:
        raise AssertionError(f"GEOMETRY visibility contract missing {token!r}")

# Table -> viewport and viewport -> table use the canonical RenderNode selection
# authority. 3D selection scrolls the row into view without changing SOURCE/LOD.
for token in (
    "function syncGeometryWorkspaceSelectionUi(",
    "syncGeometryWorkspaceSelectionUi(!!options.scrollGeometry)",
    "scrollGeometry:state.wizardStage==='geometry'",
    "row.scrollIntoView({block:'nearest'})",
):
    if token not in web:
        raise AssertionError(f"GEOMETRY selection sync missing {token!r}")

# Per-mesh graph is the stage authority. SOURCE add/replace clears only the
# affected graph; successful CHECK transitions only records that are not
# already geometry=passed, preserving accepted meshes as green.
record = cpp_function(session, "void ModelAssetEditorSession::recordMeshStageResult(")
for token in (
    'auto& value = record.stageChecks[stage];',
    'if (value == "passed")',
    "continue;",
    'value = "passed";',
    'value = "failed";',
):
    if token not in record:
        raise AssertionError(f"pending-only stage certification missing {token!r}")
if "resetMeshStageChecks(lodIndex, resident.id);" not in session and "resetMeshStageChecks(lodIndex, geometry.id);" not in session:
    raise AssertionError("SOURCE add/replace no longer resets only the affected mesh graph")
if 'else if (stage == "geometry")' not in session or 'GEOMETRY validation failed: invalid render-node geometry binding' not in session:
    raise AssertionError("existing GEOMETRY CHECK validation path was removed")

for token in (
    "GEOMETRY workspace is per-mesh",
    "stageChecks.geometry",
    "SHOW ALL / HIDE ALL",
    "existing duplicate comparison/consolidation",
):
    if token not in contract:
        raise AssertionError(f"PATCH_CONTRACT missing GEOMETRY protection {token!r}")

if 'ModelAssetEditorVersion = "0.10.67"' not in version:
    raise AssertionError("editor version is not 0.10.67")

print("[PASS] model asset editor v0.10.67 GEOMETRY per-mesh certification / selection / visibility")
