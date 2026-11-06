#!/usr/bin/env python3
"""Global 3D blank-click deselection contract."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEBUI = ROOT / "src/assets/webui/model_asset_editor.html"
web = WEBUI.read_text(encoding="utf-8", errors="replace")

required = (
    "function clearViewportSelection()",
    "editorViewState.clearSelection()",
    "state.surfaceSelectedGeometryIdsByLod.get(Number(state.activeLod))",
    "surfaceSet.clear()",
    "state.surfaceSelectionAnchorByLod.delete(Number(state.activeLod))",
    "state.semanticSelectedNodes.clear()",
    "state.selectedCollision=null",
    "state.selectedSocket=null",
    "state.selectedStructuralLink=null",
    "state.selectedStructuralProxy=null",
    "state.damageSelectionKind=null",
    "state.damageSelectionIndex=null",
    "clearEditorMeshTableFocus()",
    "highlightSelection()",
    "renderWizardPanel()",
)
for token in required:
    if token not in web:
        raise AssertionError(f"viewport blank-click deselection contract missing {token!r}")

# The pick path moved into viewport/picking.js: a hit selects and returns; a
# blank click (no hit) falls through to clearViewportSelection().
picking = (ROOT / "src/assets/webui/model_asset_editor/viewport/picking.js").read_text(encoding="utf-8")
hit_at = picking.find("if(hits.length){")
if hit_at < 0 or picking.find("clearViewportSelection();", hit_at) < 0:
    raise AssertionError("viewport blank-click deselection: picking no longer falls back to clearViewportSelection() without a hit")

# A blank viewport click is selection-only. It must never change scene/view
# authority, visibility/isolation, or camera state.
start = web.index("function clearViewportSelection()")
end = web.index("function focusActiveMeshTableSelection()", start)
body = web[start:end]
for forbidden in (
    "activeLod=",
    "sceneLod=",
    "pendingActiveLod=",
    "visibilityByLod.set",
    "visibilityByLod.delete",
    "isolationByLod",
    "state.hidden",
    "state.isolated",
    "camera.position",
    "controls.target",
    "fitView(",
):
    if forbidden in body:
        raise AssertionError(f"blank-click deselection illegally mutates view state: {forbidden!r}")

print("[PASS] model asset editor global viewport blank-click deselection")
