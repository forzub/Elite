#!/usr/bin/env python3
"""SURFACES workspace acceptance contract for the post-core-tabs baseline."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEBUI = ROOT / "src/assets/webui/model_asset_editor.html"
SESSION_CPP = ROOT / "tools/model_asset_editor/ModelAssetEditorSession.cpp"
SESSION_H = ROOT / "tools/model_asset_editor/ModelAssetEditorSession.h"
web = WEBUI.read_text(encoding="utf-8", errors="replace")
cpp = SESSION_CPP.read_text(encoding="utf-8", errors="replace")
header = SESSION_H.read_text(encoding="utf-8", errors="replace")

start = web.index("if(stage==='surfaces')")
end = web.index("if(stage==='semantics')", start)
surfaces = web[start:end]

# SURFACES has one authoritative mesh/surface table. The generic post-GEOMETRY
# mesh panel is deliberately not shown here, otherwise the same active LOD is
# presented by two competing lists again.
for token in (
    'data-wizard-groups="semantics physics damage validate build"',
    "function sharedStageMeshStage(){return ['semantics','physics','damage','validate','build']",
    'ACTIVE LOD MESHES /',
    'id="wizardSurfaceGeometryTable"',
    'id="wizardSurfaceShowAllBtn"',
    'id="wizardSurfaceHideAllBtn"',
    'data-surface-geometry-id',
    "setSurfaceGeometryVisible(g,check.checked)",
):
    if token not in web:
        raise AssertionError(f"SURFACES combined table contract missing {token!r}")

# Per-mesh state colour is evidence-driven, but NOT_CHECKED is neutral. Only an
# actual failed SURFACES check is red/brown; passed is green/dark-green. A
# failed mesh must not poison every other row merely because the stage as a
# whole did not pass.
for token in (
    "function surfaceStageVisualClass(g)",
    "if(value==='passed')return'meshStagePassed'",
    "if(value==='failed'||value==='missing')return'meshValidationPending'",
    "return'surfaceStageUnchecked'",
    '.surfaceGeometryRow.surfaceStageUnchecked{',
    '.surfaceGeometryRow.meshStagePassed.selected',
    '.surfaceGeometryRow.meshValidationPending.selected',
    "function surfaceStageGlyph(g)",
):
    if token not in web:
        raise AssertionError(f"SURFACES pass/fail/unchecked row colouring missing {token!r}")
for token in (
    'validateSurfaceGeometryStage',
    'recordSurfaceMeshStageResults',
    'record.stageChecks["surfaces"] = passed ? "passed" : "failed"',
    'if (stage == "surfaces") recordSurfaceMeshStageResults(stage != "build")',
):
    if token not in cpp and token not in header:
        raise AssertionError(f"SURFACES per-geometry CHECK evidence missing {token!r}")
for forbidden in (
    "stageClass=passed?'meshStagePassed':'meshValidationPending'",
    "intentReview?'review '",
    "bad?'badRow'",
    '.surfaceGeometryRow.review{',
    '.surfaceGeometryRow.badRow{',
):
    if forbidden in web:
        raise AssertionError(f"SURFACES incorrect/third row-colour state survived: {forbidden!r}")

# Multi-select is stage-local and does not compete with EditorViewState's
# single primary render/mesh selection. Plain click replaces, Ctrl toggles one,
# Shift fills the range from the anchor.
for token in (
    'surfaceSelectedGeometryIdsByLod:new Map()',
    'surfaceSelectionAnchorByLod:new Map()',
    'function surfaceSelectionSet(',
    'const ids=geometries.map(x=>String(x.id))',
    'ctrl=!!(event?.ctrlKey||event?.metaKey)',
    'shift=!!event?.shiftKey',
    'if(!ctrl)selected.clear();for(let i=lo;i<=hi;i++)selected.add(ids[i])',
    'if(selected.has(id)&&selected.size>1)selected.delete(id);else selected.add(id)',
    'else{selected.clear();selected.add(String(g.id));surfaceSetSelectionAnchor(g.id);}',
):
    if token not in web:
        raise AssertionError(f"SURFACES multiselect contract missing {token!r}")

# Surface intent applies to the selected group, while material editing remains
# anchored to the primary geometry. Row clicks rerender under preserveUiScroll
# but never focus/scroll the lower surface-type block.
for token in (
    'selectedTargets=selectedGeometries.length?selectedGeometries:[selected]',
    "for(const target of selectedTargets)send('set_geometry_topology_class'",
    'PRIMARY MESH ONLY',
    'el.onclick=e=>surfaceSelectGeometry(g.id,e)',
    'renderWizardPanel();highlightSelection();if(options.focusTable)',
):
    if token not in web:
        raise AssertionError(f"SURFACES group authoring contract missing {token!r}")
if 'el.onclick=e=>surfaceSelectGeometry(g.id,e,{focusTable:true})' in web:
    raise AssertionError('SURFACES table click must not force-scroll/focus another control group')

# 3D click remains single-primary and focuses the matching combined table row.
for token in (
    "if(stage==='surfaces')",
    "$('wizardSurfaceGeometryTable')?.querySelectorAll('[data-surface-geometry-id]')",
    "requestAnimationFrame(()=>focusActiveMeshTableSelection())",
    "selected.clear();selected.add(String(g.id));surfaceSetSelectionAnchor(g.id)",
):
    if token not in web:
        raise AssertionError(f"SURFACES 3D/table synchronization missing {token!r}")

# Viewport selection is an exact saturated green replacement, not a tint of
# the authored/base colour. This avoids the old white/blue -> cyan blend that
# was almost invisible on pale meshes.
for token in (
    'selectionColor=0x00a84f',
    'selectionEmissive=0x00ff70',
    'm.color?.setHex(renderPrimary?selectionColor:0x7d91a7)',
    'm.emissive?.setHex(renderPrimary?selectionEmissive:0x000000)',
    'm.color?.setHex(selectionColor);m.emissive?.setHex(selectionEmissive)',
    'm.emissiveIntensity=Math.max(.72',
):
    if token not in web:
        raise AssertionError(f"high-contrast green viewport selection contract missing {token!r}")
if 'm.color?.lerp(new THREE.Color(selectionColor)' in web:
    raise AssertionError('SURFACES selection regressed to low-contrast authored-colour tinting')

print('[PASS] model asset editor SURFACES combined table / per-mesh CHECK / multiselect / high-contrast selection')
