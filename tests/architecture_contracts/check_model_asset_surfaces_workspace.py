#!/usr/bin/env python3
"""SURFACES workspace acceptance contract for the post-core-tabs baseline."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEBUI = ROOT / "src/assets/webui/model_asset_editor.html"
web = WEBUI.read_text(encoding="utf-8", errors="replace")

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

# Per-mesh state colour is binary at this stage: passed is green, everything
# not passed remains red/brown. Review state may be explained by icon/text but
# must not introduce a third row-colour authority.
for token in (
    "stageValue=meshStageCheckValue(g,'surfaces')",
    "passed=stageValue==='passed'",
    "stageClass=passed?'meshStagePassed':'meshValidationPending'",
    '.surfaceGeometryRow.meshStagePassed.selected',
    '.surfaceGeometryRow.meshValidationPending.selected',
):
    if token not in web:
        raise AssertionError(f"SURFACES pass/fail row colouring missing {token!r}")
for forbidden in (
    "intentReview?'review '",
    "bad?'badRow'",
    '.surfaceGeometryRow.review{',
    '.surfaceGeometryRow.badRow{',
):
    if forbidden in web:
        raise AssertionError(f"SURFACES third row-colour state survived: {forbidden!r}")

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

# Selection colour is now a saturated green shared by all viewport stages.
for token in (
    'selectionColor=0x5dff9a',
    'selectionEmissive=0x0f5f31',
    'm.color?.setHex(renderPrimary?selectionColor:0x7d91a7)',
    'm.emissive?.setHex(renderPrimary?selectionEmissive:0x000000)',
    'm.color?.lerp(new THREE.Color(selectionColor),.42)',
    'm.emissive?.lerp(new THREE.Color(selectionEmissive),.82)',
    'm.emissiveIntensity=Math.max(1.35',
):
    if token not in web:
        raise AssertionError(f"green viewport selection contract missing {token!r}")

print('[PASS] model asset editor SURFACES combined table / multiselect / green viewport selection')
