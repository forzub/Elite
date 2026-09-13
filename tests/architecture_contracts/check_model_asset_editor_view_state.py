#!/usr/bin/env python3
"""EditorViewState authority / tab-neutral viewport lifecycle contract."""
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


web = text("src/assets/webui/model_asset_editor.html")
web_bundle = load_source_bundle(ROOT)
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
contract = text("tools/model_asset_editor/PATCH_CONTRACT.md")

# One authoritative owner; old stage maps may survive only as adapter names.
for token in (
    "class EditorViewState",
    "this.activeLod=0;this.sceneLod=null;this.pendingActiveLod=null;",
    "this.selectedRenderNode=null;this.selectedRenderNodeId=null;this.selectedMeshId=null;this.selectedSemanticNode=null;",
    "this.visibilityByLod=new Map();this.hiddenSemanticByLod=new Map();this.isolationByLod=new Map();",
    "this.loadedLods=new Set();this.residentLods=new Set();",
    "geometryInventoryVisibleByLod:{get:()=>editorViewState.geometryInventoryVisibility}",
    "lodPreflightVisibleByLod:{get:()=>editorViewState.lodPreflightVisibility}",
    "geometryStageVisibleByLod:{get:()=>editorViewState.geometryStageVisibility}",
    "hiddenRenderNodes:{get:()=>editorViewState.hiddenRenderNodes}",
):
    if token not in web:
        raise AssertionError(f"EditorViewState authority missing {token!r}")

for forbidden in (
    "geometryInventoryVisibleByLod:new Map()",
    "lodPreflightVisibleByLod:new Map()",
    "geometryStageVisibleByLod:new Map()",
    "maintenanceView:",
    "maintenanceScope:",
    "maintenanceWorksetBarHtml",
    "bindMaintenanceWorkset",
    "data-maint-view",
    "function resetGeometryViewportState(",
    "function resetSurfaceViewportState(",
    "function resetSemanticViewportState(",
):
    if forbidden in web:
        raise AssertionError(f"competing/local view-state mechanism survived: {forbidden!r}")

# Stage navigation is render-only: it may rebuild stage-specific preview objects,
# but it cannot pick LOD0, clear selection/visibility, or move the camera.
stage = js_function(web, "setWizardStage")
for token in (
    "const viewBefore=captureEditorViewTransition()",
    "rebuildScene(true)",
    "assertEditorViewTransitionPreserved(viewBefore",
    "assertEditorViewInvariant(",
):
    if token not in stage:
        raise AssertionError(f"tab-neutral lifecycle missing {token!r}")
for forbidden in (
    "state.activeLod=0",
    "state.selectedRenderNode=null",
    "visibilityByLod.clear",
    "hiddenSemanticByLod.clear",
    "isolationByLod.clear",
    "fitView(",
    "resetGeometryViewportState",
    "resetSurfaceViewportState",
    "resetSemanticViewportState",
):
    if forbidden in stage:
        raise AssertionError(f"tab transition still mutates persistent view state: {forbidden!r}")

snapshot = js_function(web, "captureEditorViewTransition")
for token in (
    "activeLod:state.activeLod",
    "sceneLod:state.sceneLod",
    "selectedRenderNodeId:editorViewState.selectedRenderNodeId",
    "selectedMeshId:editorViewState.selectedMeshId",
    "selectedSemanticNode:editorViewState.selectedSemanticNode",
    "visibility:[...editorViewState.visibilityByLod]",
    "hidden:[...editorViewState.hiddenSemanticByLod]",
    "isolated:[...editorViewState.isolationByLod]",
    "position:state.camera.position.toArray()",
    "quaternion:state.camera.quaternion.toArray()",
):
    if token not in snapshot:
        raise AssertionError(f"tab transition snapshot does not guard {token!r}")

invariant = js_function(web, "assertEditorViewInvariant")
for token in (
    "Number(state.sceneLod)!==Number(state.activeLod)",
    "reportEditorDiagnostic('state_invariant'",
    "throw error",
    "renderNodeGroups.length>0",
    "residentLods.has(Number(state.sceneLod))",
):
    if token not in invariant:
        raise AssertionError(f"scene/active invariant missing {token!r}")

rebuild = js_function(web, "rebuildScene")
for token in (
    "sceneResident=!!lod&&lodHasGeometryPayload(lod)",
    "editorViewState.commitSceneLod(state.activeLod)",
    "editorViewState.syncResidency(state.asset)",
    "assertEditorViewInvariant('rebuildScene')",
):
    if token not in rebuild:
        raise AssertionError(f"scene residency lifecycle missing {token!r}")

# A received payload becomes resident cache. It may become active only if it was
# the explicitly pending target, so async traffic cannot steal the viewport LOD.
handle = js_function(web, "handle")
for token in (
    "editorViewState.syncResidency(state.asset)",
    "const activate=state.pendingActiveLod===li",
    "if(activate){state.activeLod=li;state.pendingActiveLod=null;rebuildScene(true);}",
):
    if token not in handle:
        raise AssertionError(f"LOD payload activation boundary missing {token!r}")

# Physical Mesh.visible has exactly one persistent authority. Old SOURCE/LOD/
# GEOMETRY map names are adapters and must not be independently ANDed here.
visibility = js_function(web, "updateVisibility")
for token in (
    "editorViewVisible=editorViewState.renderNodeVisible(state.activeLod,i,rn)",
    "transformCarrier=!editorViewVisible&&geometrySelectionNeedsAncestor(i,lod)",
    "child.visible=editorViewVisible&&lodGeneratorNodePassesMeshFilter(rn,lodGeneratorSelectedGeometry(state.wizardStage,state.lodAnalysis,state.lodGeneratorMeshSelection,state.activeLod,activeRenderLod(state.asset?.renderLods,state.activeLod)))",
):
    if token not in visibility:
        raise AssertionError(f"authoritative viewport visibility missing {token!r}")
for forbidden in (
    "geometryInventoryNodeVisible(",
    "lodPreflightNodeVisible(",
    "geometryStageNodeVisible(",
    "geometryCompareChecked.has(i)",
    "geometryNodePassesMeshFilter(",
):
    if forbidden in visibility:
        raise AssertionError(f"competing viewport visibility owner survived: {forbidden!r}")

# SEMANTICS and later stages expose the shared LOD/mesh navigation surface.
# SURFACES owns a richer combined mesh/surface table instead of duplicating it.
for token in (
    'id="sharedStageMeshSection"',
    'data-wizard-groups="semantics physics damage validate build"',
    "function renderSharedStageMeshPanel()",
    "data-shared-mesh-lod",
    "● ПОКАЗАТЬ ВСЕ",
    "○ СПРЯТАТЬ ВСЕ",
    "check.type='checkbox'",
    "editorViewState.setRenderNodeVisible(state.activeLod,i,check.checked)",
    "row.onclick=()=>selectRenderNode(i)",
    "meshStageVisualClass(g,stage)",
    "meshStageCheckValue(g,stage).toUpperCase()",
):
    if token not in web_bundle:
        raise AssertionError(f"shared post-GEOMETRY mesh panel missing {token!r}")

# Browser diagnostics are captured before they can disappear into DevTools and
# are appended in the per-asset WORKING workspace logs directory.
for token in (
    "window.addEventListener('error'",
    "reportEditorDiagnostic('js_exception'",
    "window.addEventListener('unhandledrejection'",
    "reportEditorDiagnostic('unhandledrejection'",
    "reportEditorDiagnostic('websocket_dispatch'",
    "reportEditorDiagnostic('ui_dispatch'",
    "timestamp:new Date().toISOString()",
    "stage:state.wizardStage",
    "activeLod:Number(state.activeLod)",
    "sceneLod:state.sceneLod",
    "selectedMesh:editorViewState.selectedMeshId",
):
    if token not in web:
        raise AssertionError(f"editor diagnostic client missing {token!r}")
for token in (
    'command == "editor_ui_diagnostic"',
    'wizardLogPath("editor_ui.log")',
    'std::ofstream out(path, std::ios::app)',
    '"serverTimestampUtc"',
    '"clientTimestamp"',
    '"activeLod"',
    '"sceneLod"',
    '"selectedMesh"',
):
    if token not in session:
        raise AssertionError(f"editor diagnostic backend missing {token!r}")

for token in (
    "sole persistent browser view-state authority",
    "sceneLod == activeLod",
    "loadedLods",
    "residentLods",
    "editor_ui.log",
):
    if token not in contract:
        raise AssertionError(f"PATCH_CONTRACT missing EditorViewState rule {token!r}")

print("[PASS] model asset editor authoritative EditorViewState / tab-neutral viewport lifecycle")
