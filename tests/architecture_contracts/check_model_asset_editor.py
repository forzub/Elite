#!/usr/bin/env python3
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def require(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing {token!r}")


def body_between(body: str, start: str, end: str) -> str:
    a = body.index(start)
    b = body.index(end, a + len(start))
    return body[a:b]


# -----------------------------------------------------------------------------
# Shared v4/runtime boundary
# -----------------------------------------------------------------------------
model = text("src/model_asset/ModelAsset.h")
for forbidden in ("glad/", "GLFW", "MeshGPU", "SceneRenderer", "SpaceState"):
    if forbidden in model:
        raise AssertionError(f"shared ModelAsset leaked runtime/render dependency {forbidden!r}")

require(
    "src/model_asset/ModelAsset.h",
    "ModelAssetFormatVersion = 4",
    "struct MaterialDefinition",
    "struct RenderGeometryDefinition",
    "struct RenderNode",
    "struct RenderLod",
    "struct StateVariant",
    "struct HitRegion",
    "struct Opening",
    "struct RepairTarget",
    "std::vector<RenderLod> renderLods",
)
require(
    "src/model_asset/ModelAssetBinary.cpp",
    "ManifestMagicV4",
    "MeshMagicV4",
    "ModelAssetBinary::validate",
    "saveManifest",
    "saveLod",
    "loadManifest",
    "loadLod",
    "duplicate RenderNode id",
    "duplicate semantic Node id",
)

# The editor source path preserves authoring topology/material data and does not
# route through the runtime OBJ loader.
require(
    "tools/model_asset_editor/NativeObjImporter.cpp",
    "tinyobj::LoadObj",
    "polygonId",
    "materialIndexFor",
    "EdgeTriangulationInternal",
    "EdgeMaterialSeam",
    "EdgeNormalSeam",
)
importer = text("tools/model_asset_editor/RuntimeAssemblyImporter.cpp")
for forbidden in ("AssemblyMeshLibrary", "ObjLoader", "MeshData"):
    if forbidden in importer:
        raise AssertionError(f"editor importer depends on runtime mesh processing {forbidden!r}")

# -----------------------------------------------------------------------------
# 0.10.33 manual working-state save/restore contract
# -----------------------------------------------------------------------------
session_h = text("tools/model_asset_editor/ModelAssetEditorSession.h")
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
web = text("src/assets/webui/model_asset_editor.html")

require(
    "tools/model_asset_editor/ModelAssetEditorSession.h",
    "workingAssetPath",
    "workingEditorStatePath",
    "productionEditorStatePath",
    "saveWorkingAsset",
    "restoreWorkingAsset",
    "checkWizardStage",
    "m_editorStateDirty",
)
require(
    "tools/model_asset_editor/ModelAssetEditorSession.cpp",
    'command == "save_asset"',
    'command == "restore_working_asset"',
    'command == "check_wizard_stage"',
    "saveWorkingAsset",
    "restoreWorkingAsset",
    "buildProductionAsset",
    "loadWorkingEditorState",
    "writeWorkingEditorState",
    "loadProductionEditorState",
    "writeProductionEditorState",
)

# Removed persistence concepts must stay removed from active code/UI. This is a
# deliberate architecture prohibition, not just a hidden UX choice.
for forbidden in (
    "autosave_working",
    "create_wizard_checkpoint",
    "restore_wizard_checkpoint",
    "complete_wizard_stage",
    "save_manifest",
    "save_lod",
    "wizardCheckpointPath",
    "wizardCheckpointEditorStatePath",
    "createWizardCheckpoint",
    "restoreWizardCheckpoint",
    "writeCheckpointEditorState",
    "loadCheckpointEditorState",
    "checkpointSequenceForStage",
    "m_nextCheckpointSequence",
):
    if forbidden in session_h or forbidden in session or forbidden in web:
        raise AssertionError(f"removed persistence mechanism returned: {forbidden!r}")

# No active backend/UI checkpoint/autosave terminology either: future work must
# use the single SAVE/RESTORE working-state model.
for label, body in (("session", session), ("session header", session_h), ("web UI", web)):
    for forbidden in ("checkpoint", "rollback snapshot", "ongoing autosave"):
        if forbidden.lower() in body.lower():
            raise AssertionError(f"{label} still carries removed persistence terminology {forbidden!r}")

save_body = body_between(
    session,
    "bool ModelAssetEditorSession::saveWorkingAsset(bool quiet)",
    "bool ModelAssetEditorSession::saveAsset()",
)
for token in (
    "workingAssetPath()",
    "ModelAssetBinary::saveLod(path.string(), m_asset, i, &error)",
    "ModelAssetBinary::saveManifest(path.string(), m_asset, &error)",
    "std::filesystem::copy_file",
    "utcTimestampNow()",
    "writeWorkingEditorState(savedAtUtc, nextSaveRevision, &error)",
    "m_editorStateDirty = false",
):
    if token not in save_body:
        raise AssertionError(f"manual SAVE lost coherent working-state behavior {token!r}")
if "ensureAllLodsLoaded()" in save_body:
    raise AssertionError("manual SAVE must stay lazy and must not load every LOD")
for forbidden in ("writeProductionEditorState", "buildProductionAsset"):
    if forbidden in save_body:
        raise AssertionError(f"manual SAVE leaked across production boundary: {forbidden!r}")

restore_body = body_between(
    session,
    "bool ModelAssetEditorSession::restoreWorkingAsset()",
    "bool ModelAssetEditorSession::buildProductionAsset()",
)
for token in (
    "workingAssetPath()",
    "selectAsset(id, false)",
    "Unsaved changes were discarded",
):
    if token not in restore_body:
        raise AssertionError(f"RESTORE lost last-saved-working semantics {token!r}")
for forbidden in ("ModelAssetBinary::save", "saveWorkingAsset", "buildProductionAsset"):
    if forbidden in restore_body:
        raise AssertionError(f"RESTORE must discard/reload, not persist: {forbidden!r}")

# OPEN must prefer the one saved WORKING ASSET. First-ever production/source
# adoption may establish the initial saved baseline, but ordinary reimport is dirty.
select_body = body_between(
    session,
    "bool ModelAssetEditorSession::selectAsset(const std::string& id, bool forceReimport)",
    "bool ModelAssetEditorSession::saveWorkingAsset(bool quiet)",
)
for token in (
    "if (!forceReimport && haveWorking)",
    "Reading persistent working manifest",
    "loadWorkingEditorState",
    "const bool createInitialWorkingBaseline = !haveWorking && !forceReimport;",
    "saveWorkingAsset(true)",
    "loaded WORKING revision remains r",
):
    if token not in select_body:
        raise AssertionError(f"OPEN/REIMPORT working-state contract missing {token!r}")

# -----------------------------------------------------------------------------
# stage CHECK is persistence read-only
# -----------------------------------------------------------------------------
check_body = body_between(
    session,
    "bool ModelAssetEditorSession::checkWizardStage(const std::string& stage)",
    "bool ModelAssetEditorSession::scanRenderDuplicates(",
)
for token in (
    "validateWizardStage(stage, &validationError)",
    'value.status = passed ? "complete" : "needs_fix"',
    'if (stage == "validate") sendWizardValidationReport();',
    'if (passed && stage == "build")',
    "buildProductionAsset()",
    '"type", "wizard_stage_checked"',
    "Nothing was saved",
):
    if token not in check_body:
        raise AssertionError(f"stage CHECK contract missing {token!r}")
for forbidden in (
    "saveWorkingAsset(",
    "writeWorkingEditorState(",
    "ModelAssetBinary::save(",
    "ModelAssetBinary::saveManifest(",
    "ModelAssetBinary::saveLod(",
):
    if forbidden in check_body:
        raise AssertionError(f"stage CHECK performs persistence I/O: {forbidden!r}")
# BUILD is the terminal exception: after production bytes are written, its final
# per-mesh BUILD evidence is serialized once to the production editor sidecar.
if 'if (passed && stage == "build")' not in check_body or 'writeProductionEditorState(&finalStateError)' not in check_body:
    raise AssertionError("BUILD final production-state sidecar boundary disappeared")

# BUILD alone owns normal production package bytes and requires saved working state.
build_body = body_between(
    session,
    "bool ModelAssetEditorSession::buildProductionAsset()",
    "void ModelAssetEditorSession::synchronizeMeshSourceRecords(",
)
for token in (
    "compiledPath(m_selectedId)",
    "ModelAssetBinary::save(path.string(), m_asset, &error)",
    "production editor sidecar is finalized by checkWizardStage()",
):
    if token not in build_body:
        raise AssertionError(f"BUILD production boundary missing {token!r}")
if 'validationError = "save the current WORKING ASSET before BUILD"' not in check_body:
    raise AssertionError("BUILD no longer requires an explicitly saved WORKING ASSET")

# -----------------------------------------------------------------------------
# Global SAVE/RESTORE UI and single CHECK control
# -----------------------------------------------------------------------------
for token in (
    'id="saveBtn"',
    'id="restoreBtn"',
    "$('saveBtn').disabled=state.busy||!changed",
    "$('restoreBtn').disabled=state.busy||!changed",
    "send('save_asset')",
    "send('restore_working_asset')",
    "function wizardStageCheckControls(stage)",
    "send('check_wizard_stage',{stage})",
    "wizard_stage_checked",
):
    if token not in web:
        raise AssertionError(f"global SAVE/RESTORE/CHECK UI missing {token!r}")

for forbidden in (
    "wizardRunValidation",
    "scheduleWorkingAutosave",
    "workingAutosaveTimer",
    "packageNeedsSave",
    "saveManifestBtn",
):
    if forbidden in web:
        raise AssertionError(f"retired persistence/validation UI survived: {forbidden!r}")

# The editor is executable-owned, but it still consumes the shared WebUI kit.
# Both packed and filesystem-fallback deployments must carry those imports.
for token in ('/elite_ui.css', '/elite_ui.js'):
    if token not in web:
        raise AssertionError(f"Model Asset Editor lost shared UI kit import {token!r}")
cmake = text("CMakeLists.txt")
for token in (
    '"${ELITE_STATIC_ASSETS_DIR}/webui/elite_ui.css"',
    '"${ELITE_STATIC_ASSETS_DIR}/webui/elite_ui.js"',
    '--include elite_ui.css',
    '--include elite_ui.js',
    '"${ELITE_MODEL_ASSET_EDITOR_RUNTIME_ASSETS_DIR}/webui/elite_ui.css"',
    '"${ELITE_MODEL_ASSET_EDITOR_RUNTIME_ASSETS_DIR}/webui/elite_ui.js"',
):
    if token not in cmake:
        raise AssertionError(f"editor-owned UI pack/fallback lost shared UI kit resource {token!r}")

# The dirty flag, not file existence or stage position, is the sole enablement
# contract for both global working-state controls.
action_body = body_between(web, "function updateActionAvailability()", "function syncToggleButton(")
if "changed=hasAsset&&!!state.dirty" not in action_body:
    raise AssertionError("SAVE/RESTORE are no longer driven by the one dirty state")

# -----------------------------------------------------------------------------
# 0.10.34+ placement safety / 0.10.36 unified circular placement
# -----------------------------------------------------------------------------
for token in (
    "glm::extractEulerAngleXYZ",
    "render-node transform XYZ round-trip changed placement",
    'command == "move_render_node_delta"',
    'command == "apply_radial_render_layout"',
    "if (count == 1)",
    "closedCircle",
    "closedCircle ? count : count - 1",
    "const float angleDeg = totalAngle * static_cast<float>(i) / divisor;",
):
    if token not in session:
        raise AssertionError(f"instance/circular placement safety missing {token!r}")
if "glm::eulerAngles(" in session:
    raise AssertionError("matrix->Euler conversion regressed to glm::eulerAngles; XYZ placement can drift")
for token in (
    "wizardGeometryMoveBtn",
    "wizardGeometryRadialBtn",
    "geometryMoveDx",
    "radialLayoutAngles",
    "radialApplyBtn",
    "radialSelectedPivotInParent",
    "count===1",
    "divisor=closed?count:count-1",
    ".hidden{display:none!important}",
):
    if token not in web:
        raise AssertionError(f"instance/circular placement authoring UI missing {token!r}")
for forbidden in (
    'radialStepField',
    'radialMode',
    'move_render_node_radial',
    'create_radial_render_instances',
):
    if forbidden in web + session:
        raise AssertionError(f"split/slot-based radial workflow returned: {forbidden!r}")

# -----------------------------------------------------------------------------
# 0.10.34 list scroll stability: selection/rerender must never jump to the top
# -----------------------------------------------------------------------------
for token in (
    "const uiScrollPositions=new Map()",
    "function uiScrollContextKey(el)",
    "function captureUiScroll(root=document,target=uiScrollPositions)",
    "function restoreUiScroll(root=document,source=uiScrollPositions)",
    "let uiScrollPreserveDepth=0",
    "function preserveUiScroll(render)",
    "function renderWizardPanelContents()",
    "function renderWizardPanel(){return preserveUiScroll(renderWizardPanelContents);}",
    'data-preserve-scroll="side-panel"',
    'data-preserve-scroll="wizard-bar"',
    'data-preserve-scroll="geometry-compare"',
    'function renderGeometryCandidates(){return preserveUiScroll(renderGeometryCandidatesContents);}',
    'function renderVariantAssignment(){return preserveUiScroll(renderVariantAssignmentContents);}',
    'data-preserve-scroll="geometry-variants"',
    'data-preserve-scroll="geometry-variant-bases"',
    'data-preserve-scroll="surface-geometries"',
    'data-preserve-scroll="semantic-tree"',
    'data-preserve-scroll="semantic-bindings"',
    'data-preserve-scroll="source-change-scan"',
    'data-preserve-scroll="lod-preflight"',
    "rows.dataset.preserveScroll='unused-geometries'",
):
    if token not in web:
        raise AssertionError(f"list scroll stability contract missing {token!r}")

# The context key is deliberately asset/stage/LOD-aware: local rerenders reuse a
# position while a different render document does not inherit an unrelated one.
for token in (
    "state.asset?.assetId||'no-asset'",
    "state.wizardStage",
    "state.activeLod",
    "outer=uiScrollPreserveDepth===0",
    "captureUiScroll(document,new Map())",
    "restoreUiScroll(document,snapshot)",
    "requestAnimationFrame(()=>restoreUiScroll(document,snapshot))",
):
    if token not in web:
        raise AssertionError(f"context-aware scroll preservation missing {token!r}")

# -----------------------------------------------------------------------------
# 0.10.35 GEOMETRY workspace: one coherent workflow, readable identities
# -----------------------------------------------------------------------------
geometry_stage = web[web.index("if(stage==='geometry')"):web.index("if(stage==='surfaces')")]
for token in (
    'class="geometryStickyToolbar"',
    'data-geometry-scope="all"',
    'data-geometry-scope="changes"',
    'wizardGeometryCandidates',
    'wizardGeometryCleanBtn',
    'wizardExtraMeshTable',
    'wizardBaseReplacementTable',
    'wizardGeometryEditor',
    "wizardStageCheckControls('geometry')",
    'geometryStatsHtml(lod)',
):
    if token not in geometry_stage:
        raise AssertionError(f"0.10.35 GEOMETRY workflow missing {token!r}")

for token in (
    'grid-template-columns:28px 28px 24px minmax(0,1fr) 40px 42px',
    'compareMetricBadge',
    'compareMeshName',
    "statusIcon='★'",
    "statusIcon='✓'",
):
    if token not in web:
        raise AssertionError(f"0.10.35 identity-first GEOMETRY table lost compact metrics/status contract {token!r}")

for token in (
    'compareShowToggle',
    'compareReferenceToggle',
    'wizardGeometryMoveBtn',
    'wizardGeometryDuplicateBtn',
    'wizardGeometryRadialBtn',
    'wizardGeometryBreakBtn',
    'wizardGeometryDeleteBtn',
):
    if token not in web:
        raise AssertionError(f"0.10.35 GEOMETRY selector/editor control missing {token!r}")

for forbidden in (
    'wizardGeometryMainMeshes',
    'wizardGeometryExtraMeshes',
    'geometryInstanceSelection',
    'LOD GEOMETRY / PREVIEW',
    'INSTANCE / ARRAY AUTHORING',
    'partMaintenanceBlock',
):
    if forbidden in geometry_stage:
        raise AssertionError(f"historical duplicate GEOMETRY panel returned: {forbidden!r}")

order = [
    geometry_stage.index('wizardGeometryCandidates'),
    geometry_stage.index('wizardExtraMeshTable'),
    geometry_stage.index('wizardGeometryEditor'),
    geometry_stage.index("wizardStageCheckControls('geometry')"),
    geometry_stage.index('geometryStatsHtml(lod)'),
]
if order != sorted(order):
    raise AssertionError("GEOMETRY workflow order regressed: compare -> replacements -> edit -> CHECK -> stats")

if 'data-wizard-groups="lods geometry"' in web:
    raise AssertionError("Render LOD files panel leaked back into GEOMETRY")
if 'id="renderInspectorSection" class="section" data-wizard-groups="geometry' in web:
    raise AssertionError("detached selected-element inspector leaked back into GEOMETRY")

for token in (
    '.geometryStickyToolbar{position:sticky',
    '.compareNode{white-space:normal',
    '.compareSource{white-space:normal;overflow-wrap:anywhere',
    '.extraMeshRow .sourceName{white-space:normal',
    '.baseReplaceIdentity .source{',
    'function geometrySelectionNeedsAncestor(index,lod)',
    "state.geometryCompareChecked.size>0",
    'geometrySelected=!geometryGroupFilter||state.geometryCompareChecked.has(i)',
    'function renderGeometryEditor()',
    'function geometryStatsHtml(lod)',
):
    if token not in web:
        raise AssertionError(f"readable/isolated GEOMETRY workspace contract missing {token!r}")

# -----------------------------------------------------------------------------
# 0.10.37 SEMANTICS workspace: tree-first link authoring + 3D graph preview
# -----------------------------------------------------------------------------
semantic_stage = web[web.index("if(stage==='semantics')"):web.index("if(stage==='physics')")]
for token in (
    'semanticsWorkspaceWide',
    'data-semantic-relation-select',
    'draggable="true"',
    'semanticReparentSelection',
    'set_node_parents',
    'semanticGraphEnabled',
    'semanticGraphExplode',
    'semanticBindingSummaryHtml',
    'REPAIR / РЕДАКТИРОВАТЬ VISUAL BINDINGS',
    "wizardStageCheckControls('semantics')",
):
    if token not in semantic_stage and token not in web:
        raise AssertionError(f"0.10.37 SEMANTICS tree/link workflow missing {token!r}")
for token in (
    'function applySemanticGraphExplode(',
    'function rebuildSemanticGraphGizmos(',
    'semanticLinkChildIndex',
    'state.semanticSelectedNodes',
    'semanticTopLevelSelected()',
    'semanticCanUseParent(child,target)',
    '.semanticTreeRow.selected{background:#17334a!important',
    '.semanticAssetSpaceRow{display:grid',
    '.wizardLodSticky{position:sticky',
):
    if token not in web:
        raise AssertionError(f"0.10.37 SEMANTICS visual/selection contract missing {token!r}")
for token in (
    'reparentSemanticNodesPreserveWorld',
    'semanticNodeWorldTransform',
    'if (command == "set_node_parents")',
):
    if token not in session:
        raise AssertionError(f"0.10.37 SEMANTICS backend contract missing {token!r}")
if 'semanticRelationButton' in semantic_stage:
    raise AssertionError('old detached semantic relation button panel returned; link type belongs in the tree row')
if '<div class="semanticBindingTable"' in semantic_stage and '<details class="semanticBindingEditor"' not in semantic_stage:
    raise AssertionError('full Render binding table is no longer allowed as a permanently open top-level SEMANTICS panel')

# -----------------------------------------------------------------------------
# 0.10.42 semantic/render lifecycle integrity + single-authority persistent 3D graph preview
# -----------------------------------------------------------------------------
# GEOMETRY may duplicate visual RenderNodes, but it must never silently clone
# semantic identity/state scope. Every new visual copy starts explicitly UNBOUND.
for command, end_command in (
    ('if (command == "duplicate_render_node_instance")', 'if (command == "move_render_node_delta")'),
    ('if (command == "apply_radial_render_layout")', 'if (command == "delete_render_node")'),
):
    block = body_between(session, command, end_command)
    for token in ('clone.semanticNodeIndex = NoIndex;', 'clone.activeStates.clear();'):
        if token not in block:
            raise AssertionError(f"0.10.40 visual copy inherited semantic identity in {command}: missing {token!r}")

for token in (
    'semanticSelectionModeFromEvent',
    "e?.shiftKey",
    "'add-range'",
    "'range'",
    'semanticSelectionAnchor',
    'semanticBindingPickTarget',
    '◎ НАЗНАЧИТЬ ЭТОЙ PART VISUAL ИЗ 3D',
    'function semanticGraphLayoutOffsets(',
    'semanticCanonicalNodeAnchorMap',
    'semanticCanonicalRenderWorldMatrices',
    'geometry.minBounds',
    'geometry.maxBounds',
    'semanticGraphGroup:new THREE.Group()',
    'semanticJointGizmoGroup:new THREE.Group()',
    'semanticGraphNodeObjects:new Map()',
    'ensureSemanticGraphObjects',
    'semanticGraphDisplayAnchorMap',
    'markHidden',
    'semantic_tree_patch',
    'applySemanticTreePatch',
    'scheduleSemanticGraphPreview',
    'function semanticRenderBaseMatrix(',
    'function restoreSemanticPreviewMatrices()',
    'marker.userData.semanticNodeIndex=i',
    'semanticSelectNode(si,semanticSelectionModeFromEvent(ev))',
    'function updateSemanticMotionAnimation(ts)',
    '▶ ВРАЩАТЬ',
    '⚡ ОТОДВИНУТЬ / ПРОВЕРИТЬ ОТРЫВ',
    '⚠ 0 VIS',
    'data-semantic-toggle',
    "f<.28?'before':f>.72?'after':'inside'",
    'semanticTreeOrder',
    'function semanticRefreshSelectionUi()',
):
    if token not in web:
        raise AssertionError(f"0.10.40 SEMANTICS lifecycle/tree/preview contract missing {token!r}")

# Selection is a hot path: changing selection must not destroy/recreate the whole
# semantic tree/panel. Structural edits may rebuild it, ordinary clicks may not.
selection_block = body_between(web, "function semanticSelectNode(index,mode='single')", "function semanticReparentSelection")
if 'renderWizardPanel()' in selection_block:
    raise AssertionError('SEMANTICS selection regressed to full wizard-panel DOM rebuild')
if 'semanticRefreshSelectionUi()' not in selection_block:
    raise AssertionError('SEMANTICS selection lost partial selection refresh')

# The incoming-link selector must visually precede the child identity. Runtime
# node vector order is not presentation order: before/after DnD is editor-only.
tree_row_start = web.index("const roots=semanticRootIndices(),assetSpaceCollapsed=state.semanticCollapsed.has('__ASSET_SPACE__')")
tree_row_end = web.index('const selectedPanels=', tree_row_start)
tree_row = web[tree_row_start:tree_row_end]
if '${toggle}${relation}<span class="name">' not in tree_row:
    raise AssertionError("SEMANTICS toggle/link controls no longer precede the child name")
if '${count} RN' in tree_row or '>0 RN<' in tree_row:
    raise AssertionError("opaque RN count leaked back into the primary semantic tree; use VIS")

for token in (
    '.semanticTreeRow{--tree-depth:0',
    'calc(7px + var(--tree-depth)*28px)',
    '.semanticTreeRow.dragBefore',
    '.semanticTreeRow.dragAfter',
    '.semanticTreeRow.dragInside',
):
    if token not in web:
        raise AssertionError(f"SEMANTICS readable/reorderable tree styling missing {token!r}")

# 0.10.42 interaction performance/collapse/graph guards. Semantic-only tree edits must
# never fall back to full asset metadata serialization (which scans geometry
# triangles for material statistics), and collapsed descendants must stay hidden.
reparent_block = body_between(session, 'if (command == "set_node_parents")', 'if (command == "clean_legacy_semantics")')
if 'sendSemanticTreePatch();' not in reparent_block:
    raise AssertionError('SEMANTICS reparent lost bounded semantic_tree_patch publication')
if 'sendAssetMetadata();' in reparent_block:
    raise AssertionError('SEMANTICS reparent regressed to full asset metadata serialization')
semantic_patch_block = body_between(session, 'void ModelAssetEditorSession::sendSemanticTreePatch()', 'void ModelAssetEditorSession::sendSurfaceMetadataPatch')
for token in ('"semantic_tree_patch"', 'serializeSemanticNodes()', 'serializeSemanticTreeOrder()'):
    if token not in semantic_patch_block:
        raise AssertionError(f'bounded semantic tree patch missing {token!r}')
for forbidden in ('serializeAssetMetadata()', '.triangles', 'materialUsage'):
    if forbidden in semantic_patch_block:
        raise AssertionError(f'semantic tree patch must not scan full render metadata: {forbidden!r}')

tree_rows_block = body_between(web, 'function semanticTreeRows()', 'function semanticDescendantSet')
for token in ('markHidden', 'if(collapsed){for(const child of kids)markHidden(child);return;}'):
    if token not in tree_rows_block:
        raise AssertionError(f'collapsed semantic subtree guard missing {token!r}')
anchor_block = body_between(web, 'function semanticCanonicalNodeAnchorMap()', 'function semanticGraphDirection')
for token in ('semanticCanonicalRenderWorldMatrices', 'geometry.minBounds', 'geometry.maxBounds', 'applyMatrix4(world)'):
    if token not in anchor_block:
        raise AssertionError(f'canonical semantic graph anchor missing {token!r}')
for forbidden in ('computeBoundingBox', 'mesh.geometry', 'state.meshObjects', 'group.matrixWorld'):
    if forbidden in anchor_block:
        raise AssertionError(f'3D semantic graph canonical anchor depends on mutable viewport state: {forbidden!r}')

# The graph overlay is persistent. Slider/motion preview may update positions, but
# must not destroy/recreate dozens of THREE geometries/materials every animation
# frame. Canonical anchors are the single source for both mesh explode and overlay.
for token in (
    'semanticGraphGroup:new THREE.Group()',
    'semanticJointGizmoGroup:new THREE.Group()',
    'semanticGraphNodeObjects:new Map()',
    'semanticGraphLinkObjects:new Map()',
    'function ensureSemanticGraphObjects()',
    'function semanticGraphDisplayAnchorMap(',
    'line.frustumCulled=false',
    'applySemanticGraphExplode(canonicalAnchors)',
    'rebuildSemanticGraphGizmos(canonicalAnchors)',
    'applySemanticMotionPreview(false)',
    'graphExplode.onchange=()=>applySemanticMotionPreview(true)',
):
    if token not in web:
        raise AssertionError(f'persistent semantic graph preview contract missing {token!r}')

graph_rebuild = body_between(web, 'function rebuildSemanticGraphGizmos(', 'function rebuildSemanticGizmos(')
for forbidden in ('new THREE.BoxGeometry', 'clearSemanticGizmos()', 'clearGroup('):
    if forbidden in graph_rebuild:
        raise AssertionError(f'semantic graph hot update reallocates/destroys overlay objects: {forbidden!r}')

joint_rebuild = body_between(web, 'function rebuildSemanticGizmos(', 'function bindSemanticMotionControls')
if 'clearSemanticGizmos()' in joint_rebuild:
    raise AssertionError('joint gizmo refresh destroys persistent semantic graph overlay')
if 'clearSemanticJointGizmos()' not in joint_rebuild:
    raise AssertionError('joint gizmo refresh lost isolated joint-overlay cleanup')

# Camera orbit must not execute selection/raycast work on pointer-down; a click
# is distinguished from a drag by a small movement threshold.
if "addEventListener('pointerdown',pick)" in web:
    raise AssertionError('viewport selection fires on camera-drag pointerdown')
for token in ("pickStart={id:ev.pointerId", 'Math.hypot(', 'if(moved<=4)pick(ev)'):
    if token not in web:
        raise AssertionError(f'viewport click-vs-orbit guard missing {token!r}')

semantic_lifecycle = text('src/model_asset/ModelAssetSemantics.cpp')
semantic_lifecycle_h = text('src/model_asset/ModelAssetSemantics.h')
for token in (
    'struct SemanticNodeUsage',
    'isOrphanCandidate() const',
    'SemanticEraseResult',
    'eraseSemanticNode',
):
    if token not in semantic_lifecycle_h:
        raise AssertionError(f"shared semantic lifecycle API missing {token!r}")
for token in (
    'renderNode.semanticNodeIndex = NoIndex;',
    'renderNode.activeStates.clear();',
    'remapNodeIndex',
    'legacySourceBootstrapCollisions',
    'cannot delete semantic node with children',
):
    if token not in semantic_lifecycle:
        raise AssertionError(f"shared semantic lifecycle implementation missing {token!r}")

for token in (
    'if (command == "delete_semantic_node")',
    'inspectSemanticNodeUsage(m_asset',
    'orphan semantic parts=',
    'm_semanticChildOrder',
    'parentChanged ? "Reparented " : "Reordered "',
    '" in editor tree only"',
):
    if token not in session:
        raise AssertionError(f"0.10.40 SEMANTICS backend lifecycle/order contract missing {token!r}")
for forbidden in ('if (command == "set_node_parent")', 'if (command == "delete_node")'):
    if forbidden in session:
        raise AssertionError(f"obsolete duplicate SEMANTICS backend path survived: {forbidden}")

source_importer = text('tools/model_asset_editor/SourceFolderImporter.cpp')
if 'asset.collisionVolumes.push_back' in source_importer:
    raise AssertionError('SOURCE importer still authors PHYSICS collision volumes')
if 'Collision/physics authoring belongs exclusively to the PHYSICS stage.' not in source_importer:
    raise AssertionError('SOURCE/PHYSICS stage ownership boundary is not documented in importer')

model_tests = text('tests/model_asset/ModelAssetBinaryTests.cpp')
for token in (
    'testSemanticLifecycleIntegrity',
    'semantic erase did not report/unbind all visual owners',
    'legacy bootstrap collision hid a dead semantic orphan',
    'semantic erase silently deleted owned gameplay payload without confirmation',
):
    if token not in model_tests:
        raise AssertionError(f"semantic lifecycle scenario regression test missing {token!r}")


# Runtime semantic-graph integrity. Undefined legacy subtree helpers must
# never reach an interaction path. Hit volumes are a global viewport overlay
# when the toolbar toggle is enabled, while collision EDIT/PICK ownership stays
# in PHYSICS/DAMAGE so SEMANTICS selection is not intercepted.
if 'isDescendant(' in web:
    raise AssertionError('undefined legacy isDescendant call survived in Model Asset Editor WebUI')
for token in (
    'function semanticIsDescendant(',
    'semanticIsDescendant(i,Number(index))',
    'function updateSemanticCollisionTransforms()',
    "if((state.wizardStage==='physics'||state.wizardStage==='damage')&&$('hitToggle').checked)",
    "function rebuildCollisions(){clearGroup(state.collisionGroup);if(!state.asset||!activeRenderLod()?.loaded||!$('hitToggle').checked)return;",
    'if(!n||Number(n.parentIndex)<0)return;',
):
    if token not in web:
        raise AssertionError(f'semantic graph/global-overlay ownership contract missing {token!r}')
if "ownsCollisionPreview=state.wizardStage==='physics'||state.wizardStage==='damage'" in web:
    raise AssertionError('hit-volume visibility is still incorrectly stage-gated despite a global toolbar toggle')

motion_block = body_between(web, 'function applySemanticMotionPreview(', 'function resetSemanticMotionPreview')
if 'updateSemanticCollisionTransforms();' not in motion_block:
    raise AssertionError('semantic preview no longer keeps owned PHYSICS/DAMAGE collision overlays coherent')

selection_refresh = body_between(web, 'function semanticRefreshSelectionUi()', 'function semanticSelectNode(')
if 'applySemanticMotionPreview(true);' not in selection_refresh:
    raise AssertionError('semantic selection refresh may drop an active graph explode/motion preview')
if '}else applySemanticMotionPreview();highlightSelection();}' in selection_refresh:
    raise AssertionError('selected semantic node still skips graph-preview reapplication')



# 0.10.45 semantic authoring UX / radial-support graph contract.
# Explode remains root-centered, but displacement is based on the far support
# point of each visual bounds along the radial ray rather than center distance.
# Joint pivot remains link-owned and is authored through explicit presets/3D pick.
if 'root=Number(nodes[i].parentIndex)<0;color=' in web:
    raise AssertionError('semantic graph still assigns to an undeclared color variable')
for token in (
    'function semanticGraphRootCenter(',
    'function semanticGraphRadialMetrics(',
    'farDistance:centerDistance',
    'metric.farDistance=Math.max(metric.farDistance,projected)',
    'far*1.20',
    'marker.visible=false;',
    'line.visible=false;',
    'semanticJointPivotPickTarget',
    'function semanticSetJointPivotWorld(',
    '◎ ORIGIN РОДИТЕЛЯ',
    '◎ ЦЕНТР VISUAL CHILD',
    '◎ УКАЗАТЬ В 3D',
    'СКОРОСТЬ ПРЕДПРОСМОТРА, °/с · editor-only',
    'СОХРАНЯЕМЫЕ ПАРАМЕТРЫ ВРАЩЕНИЯ · runtime',
    'Номинальная скорость, °/с',
    'Сила разрушения, N',
    'Момент разрушения, N·m',
    'VISUAL REPRESENTATION / LOD BINDINGS',
    'ДОПОЛНИТЕЛЬНО · SEMANTIC FRAME',
):
    if token not in web:
        raise AssertionError(f'0.10.45 semantic authoring/radial-support contract missing {token!r}')


# 0.10.46 exploded-motion composition / transactional joint authoring.
require("src/model_asset/ModelAsset.h", "Revolute ranges spanning 360 degrees or more are continuous rotation.")
require("src/assets/webui/model_asset_editor.html", "fullCircle=hi-lo>=360-.001")
require("src/assets/webui/model_asset_editor.html", "if(fullCircle){const span=hi-lo;while(next>hi)next-=span;while(next<lo)next+=span;}else if(next>=hi)")
# Explode is an editor-only spatial preview. Joint motion must be applied AFTER
# explode so the exploded subtree behaves as one rigid assembly around the one
# authored joint pivot: R_joint * T_explode * M, never T_explode * R_joint * M.
motion_block = body_between(web, 'function applySemanticMotionPreview(', 'function resetSemanticMotionPreview')
if motion_block.find('applySemanticGraphExplode(canonicalAnchors);') > motion_block.find('const delta=semanticPreviewDeltaWorld()'):
    raise AssertionError('semantic motion is still composed before graph explode')

display_block = body_between(web, 'function semanticDisplayWorldMatrix(', 'function restoreSemanticPreviewMatrices')
if display_block.find('makeTranslation(offset.x,offset.y,offset.z).multiply(out)') > display_block.find('semanticPreviewDeltaWorld().multiply(out)'):
    raise AssertionError('semantic display matrix still composes explode after joint motion')

graph_anchor_block = body_between(web, 'function semanticGraphDisplayAnchorMap(', 'function rebuildSemanticGraphGizmos')
if graph_anchor_block.find('if(offset)p.add(offset);') > graph_anchor_block.find('p.applyMatrix4(delta)'):
    raise AssertionError('semantic graph markers still compose explode after joint motion')

joint_gizmo_block = body_between(web, 'function rebuildSemanticGizmos(', 'function bindSemanticMotionControls')
for token in (
    'canonicalWorld=semanticWorldMatrix(state.selectedNode)',
    'displayWorld=semanticDisplayWorldMatrix(state.selectedNode)',
    'applyMatrix4(canonicalWorld)',
    'setFromMatrixPosition(displayWorld)',
):
    if token not in joint_gizmo_block:
        raise AssertionError(f'joint gizmo lost canonical-pivot/display-child split: {token!r}')

for token in (
    "const runtimeRotationUi=jointType==='revolute'?",
    "const breakUi=j.breakable?",
    'ПРОЧНОСТЬ СВЯЗИ · сохраняется для runtime physics/damage',
    '✓ СОХРАНИТЬ RUNTIME ПАРАМЕТРЫ СВЯЗИ',
    "if(rot){overrides.axis=readVec('sja')",
    "if(detach){overrides.breakForceN=Number($('sjForce').value)",
):
    if token not in web:
        raise AssertionError(f'contextual joint-runtime editor contract missing {token!r}')

set_joint_block = body_between(session, 'if (command == "set_joint")', 'if (command == "set_physics")')
for token in (
    'auto next = m_asset.nodes[index].joint;',
    'const bool revolute = next.type == model_asset::JointType::Revolute;',
    'const float axisLengthSq = glm::dot(next.axis, next.axis);',
    '(revolute && (!std::isfinite(axisLengthSq) || axisLengthSq <= 1.0e-8f))',
    'if (revolute)',
    'next.minAngleDeg > next.maxAngleDeg',
    'm_asset.nodes[index].joint = next;',
):
    if token not in set_joint_block:
        raise AssertionError(f'transactional set_joint validation missing {token!r}')
if 'glm::normalize(jsonVec3' in set_joint_block:
    raise AssertionError('set_joint can still normalize an unchecked zero axis into NaNs')

if 'semantic joint pivot/axis/runtime rate/limits/break thresholds lost in binary round trip' not in model_tests:
    raise AssertionError('v4 binary round trip no longer locks persisted NodeJoint runtime fields')

# -----------------------------------------------------------------------------
# 0.10.47 dual semantic views / structural graph / physical-size authoring
# -----------------------------------------------------------------------------
for token in (
    "struct PhysicalSizeProfile",
    "struct StructuralDamageProxy",
    "struct StructuralLinkDefinition",
    "std::vector<StructuralLinkDefinition> structuralLinks",
    "std::string interfaceProfile",
    "float previewFovDeg",
):
    if token not in model:
        raise AssertionError(f"0.10.47 asset-data contract missing {token!r}")

binary = text("src/model_asset/ModelAssetBinary.cpp")
for token in (
    "{{{'S','I','Z','E'}}, writePhysicalSizeV4, readPhysicalSizeV4}",
    "{{{'S','M','E','T'}}, writeSocketMetadataV4, readSocketMetadataV4}",
    "{{{'S','T','R','L'}}, writeStructuralLinksV4, readStructuralLinksV4}",
    "writePhysicalSizeV4",
    "writeSocketMetadataV4",
    "writeStructuralLinksV4",
):
    if token not in binary:
        raise AssertionError(f"0.10.47 additive v4 persistence contract missing {token!r}")

for token in (
    'command == "set_physical_size_profile"',
    'command == "apply_physical_size"',
    'const float scale = profile.targetMeters / current;',
    "scaleModelAssetUniform(m_asset, scale)",
    "semanticBoundarySegments",
    "makeStructuralProxySeed",
    'command == "add_structural_link"',
    'command == "set_structural_link"',
    'command == "set_structural_proxy"',
):
    if token not in session:
        raise AssertionError(f"0.10.47 editor backend contract missing {token!r}")

for token in (
    "TREE · СБОРКА / КИНЕМАТИКА",
    "GRAPH · КОНСТРУКЦИОННЫЕ СВЯЗИ",
    "NEW LINK · 3D PICK A ↔ B",
    "WELD SEAM · auto hit-capsule",
    "Exploded viewport никогда не является coordinate authority.",
    "PHYSICAL SIZE · UNIFORM ASSET SCALE",
    "APPLY AFTER SOURCE REIMPORT",
    "VIEW FROM SOCKET",
    "INTERFACE PROFILE",
):
    if token not in web:
        raise AssertionError(f"0.10.47 TREE/GRAPH/size/socket UI contract missing {token!r}")

importer_contract = text("tools/model_asset_editor/RuntimeAssemblyImporter.cpp")
for token in (
    "logicalDimensions.scaleReference",
    "PhysicalSizeAxis::Z",
    "asset.physicalSize.autoApplyOnSourceImport = true",
):
    if token not in importer_contract:
        raise AssertionError(f"legacy logical-size adoption contract missing {token!r}")

for token in (
    "physical-size profile lost in v4 SIZE chunk round trip",
    "socket metadata lost in additive v4 SMET chunk round trip",
    "structural graph / damage proxy lost in v4 STRL chunk round trip",
    "semantic erase did not remap surviving structural graph indices",
):
    if token not in model_tests:
        raise AssertionError(f"0.10.47 C++ regression anchor missing {token!r}")

# -----------------------------------------------------------------------------
# Capability registry: every protected capability must point to live tokens.
# This keeps broad regression coverage without accumulating version-specific
# archaeology in one giant hand-written test.
# -----------------------------------------------------------------------------
cap_file = ROOT / "tools/model_asset_editor/EDITOR_CAPABILITIES.json"
capabilities = json.loads(cap_file.read_text(encoding="utf-8"))
ids = [item["id"] for item in capabilities["protected_capabilities"]]
if len(ids) != len(set(ids)):
    raise AssertionError("duplicate protected capability id")
for required_id in (
    "geometry_instance_fit",
    "independent_render_lods",
    "semantic_damage_states",
    "source_reimport_read_only",
    "working_state_controls",
    "incremental_editor_sync",
    "source_render_variants",
    "model_preflight",
    "lod_generator_preview",
    "lod_generator_authoring",
    "surface_authoring",
    "stable_list_scroll",
    "geometry_workspace_flow",
    "semantic_tree_link_authoring",
    "structural_graph_authoring",
    "physical_size_socket_profiles",
):
    if required_id not in ids:
        raise AssertionError(f"protected capability missing {required_id!r}")

for capability in capabilities["protected_capabilities"]:
    for contract_name, contract in capability.get("contracts", {}).items():
        path = contract.get("path")
        if not path:
            raise AssertionError(f"{capability['id']}/{contract_name}: contract path missing")
        body = text(path)
        # Test contracts may point back to this architecture test or to dedicated
        # C++ regression suites. Their execution is owned by the normal test runner;
        # here we validate the production/data/UI capability anchors.
        if contract_name == "test":
            continue
        for token in contract.get("tokens", []):
            if token not in body:
                raise AssertionError(
                    f"{capability['id']}/{contract_name}: {path} missing protected token {token!r}"
                )

# Ship source catalog must be filesystem-visible instead of collapsing every
# Cobra source folder into one opaque logical entry. The folder used by the
# current game runtime keeps the canonical cobra_mk1 identity and registry
# semantics; sibling folders remain independently selectable source assets.
for token in (
    "discoverShipSourceDirectories",
    "runtimeAssemblySourceDirectory",
    "catalogIdForShipFolder",
    "CatalogSourceAuthority::RuntimeAssembly",
    "CatalogSourceAuthority::Folder",
    'std::string("Cobra Mk.I — ") + directory.filename().string()',
):
    if token not in session:
        raise AssertionError(f"ship source catalog contract missing {token!r}")

# Whole-mesh orientation repair is an editor-authoring override layered after
# automatic canonical PREPARE. The runtime stores only corrected mesh winding;
# the sidecar retains the decision and source revision so reimport can mark it stale.
canonical_header = text("tools/model_asset_editor/CanonicalMeshBuilder.h")
for token in (
    "void flipMeshOrientation(MeshLod& mesh);",
):
    if token not in canonical_header:
        raise AssertionError(f"manual mesh orientation primitive missing {token!r}")
for token in (
    "MeshOrientationOverrideRecord",
    "meshOrientationOverrides",
    'command == "set_geometry_orientation_override"',
    "flipMeshOrientation(geometry.mesh);",
    "source revision changed; reimport and PREPARE",
    "preserveOrientationOverrides",
):
    if token not in session and token not in session_h:
        raise AssertionError(f"manual mesh orientation authoring contract missing {token!r}")
for token in (
    "data-preflight-orientation",
    "FLIP ORIENTATION",
    "MANUAL FLIP · STALE",
):
    if token not in web:
        raise AssertionError(f"manual mesh orientation UI contract missing {token!r}")


# LOD mesh table is always available for navigation/repair; a previous ANALYZE
# result stays visible as cached evidence across same-asset geometry refreshes.
# Render-node visibility is editor-only and independent from semantic hiding.
for token in (
    "hiddenRenderNodes:new Set()",
    "preflightMeshToolbarHtml",
    "preflightFlipSelected",
    "selectedMeshOrientationAction",
    "HIDE SELECTED",
    "SHOW SELECTED",
    "SHOW ALL",
    "renderModelPreflightInventory",
    "cachedAfterGeometryChange",
):
    if token not in web:
        raise AssertionError(f"LOD mesh visibility/cached-preflight UI contract missing {token!r}")

# Keep the exact current editor version guarded.
require("tools/model_asset_editor/EditorVersion.h", 'ModelAssetEditorVersion = "0.10.59"')

# These marker phrases are intentionally referenced by the capability registry.
manual_working_state_marker = "manual working-state save/restore contract"
stage_check_marker = "stage CHECK is persistence read-only"

for token in (
    "renderNodeForPreflightGeometry",
    "syncPreflightSelectionUi",
    "scrollPreflightRowIntoView",
    "selectRenderNode(i,options={})",
    "scrollPreflight:state.wizardStage==='lods'",
    "grid-template-columns:1fr 1fr",
):
    if token not in web:
        raise AssertionError(f"0.10.48 unified LOD mesh-selection UI contract missing {token!r}")

# Runtime-backed assets with a known source folder must discover every authored
# render LOD from LOD<N> directories. The registry remains semantic/LOD0
# authority and is explicitly not a whitelist for higher render documents.
for token in (
    "discoverSourceFolderOrdinaryMeshes",
    "folderOwnsHigherLods",
    "buildIndependentRenderLodsFromLegacy(asset);",
    "asset.renderLods.resize(1);",
    "for (std::size_t level = 1; level <= highest; ++level)",
    "Higher LOD topology may be",
):
    if token not in importer:
        raise AssertionError(f"0.10.49 runtime all-LOD source discovery contract missing {token!r}")
if "m_sourceAssetsRoot, it->sourceDirectory, it->type" not in session:
    raise AssertionError("0.10.49 runtime importer is not scoped to the selected source directory")


html_050 = text("src/assets/webui/model_asset_editor.html")
for token in [
    'MESHES · ACTIVE LOD',
    'data-struct-mesh-ri',
    'id="structMakeRoot"',
    'A ← SELECTED',
    'B ← SELECTED',
    'structuralPickNode(si,ri)',
    'structEndpointPair',
    'structNewLinkOptions',
]:
    if token not in html_050:
        raise AssertionError(f"0.10.50 structural graph mesh/root selection contract missing {token!r}")
if 'EXPLODE ROOT <select id="structGraphRoot"' in html_050:
    raise AssertionError("0.10.50 must not restore the structural root dropdown; root is selected from the mesh table/3D selection")

for token in (
    "primaryVisual?0xd6ff54",
    "semanticAdditional?0xffd166",
    "semanticSelected?0x365f66",
    "selectedMarker=selected&&!hasVisual",
):
    if token not in html_050:
        raise AssertionError(f"0.10.53 semantic primary-mesh visual contract missing {token!r}")

for token in (
    "cleanLegacySyntheticVisualSemanticNodes",
    "geometryBindingWithLegacyIdentity",
    "parentTransformOnlyBinding",
    "RenderNode now owns that visual binding",
):
    if token not in text("src/model_asset/ModelAssetSemantics.cpp") and token not in importer:
        raise AssertionError(f"0.10.52 legacy semantic cleanup contract missing {token!r}")
for token in (
    'command == "clean_legacy_semantics"',
    'transform parent chains and structural graph links were not changed',
):
    if token not in session:
        raise AssertionError(f"0.10.52 TREE legacy cleanup backend contract missing {token!r}")
for token in (
    'id="semanticCleanLegacyTree"',
    'id="semanticCleanLegacyGraph" disabled',
    "send('clean_legacy_semantics')",
    'GRAPH cleanup intentionally not wired yet',
):
    if token not in web:
        raise AssertionError(f"0.10.52 TREE/GRAPH cleanup UI contract missing {token!r}")

for token in (
    "semanticStaticFlattenCandidates()",
    "semanticMoveSelectionToAssetSpace()",
    "semanticFlattenStaticTree()",
    'id="semanticSelectionToAssetSpace"',
    'id="semanticFlattenStaticTree"',
    "FLATTEN STATIC → ASSET SPACE",
    "◇ ASSET SPACE · implicit transform parent",
    "TRANSFORM FOREST:",
    "STRUCTURAL GRAPH is not changed",
):
    if token not in web:
        raise AssertionError(f"0.10.54 asset-space transform-forest UI contract missing {token!r}")
for token in (
    "staticSemanticFlattenCandidates",
    'command == "flatten_static_semantic_tree"',
    "reparentSemanticNodesPreserveWorld(m_asset, {index}, NoIndex);",
    "Semantic transforms form a forest in asset space",
    "STRUCTURAL GRAPH unchanged",
):
    if token not in session:
        raise AssertionError(f"0.10.54 asset-space transform-forest backend contract missing {token!r}")
for forbidden in (
    "TRANSFORM TREE AUDIT · READ-ONLY",
    "semanticTransformAuditHtml()",
    "asset must have exactly one semantic root; found",
    'command == "create_semantic_asset_root"',
):
    if forbidden in web or forbidden in session:
        raise AssertionError(f"0.10.54 obsolete unique-root/audit contract returned: {forbidden!r}")

for token in (
    "if(state.wizardStage!=='semantics')return;",
    "replaceSourcePartByPath",
    "resident render geometry verified unchanged",
    "collect mtllib declarations",
):
    if token not in web and token not in session and token not in session_h:
        raise AssertionError(f"0.10.55 semantic hotfix contract missing {token!r}")
if "const forceVisible=state.wizardStage==='semantics'" in web:
    raise AssertionError("0.10.55 must not allow socket markers to leak outside SEMANTICS through the global toggle")

# 0.10.58 SOURCE lifecycle: Folder geometry authority is independent from
# runtime semantic bootstrap; scan is exact-hash synchronization; folder OPEN
# keeps every declared LOD resident; one WORKING revision is visible in status.
scan_body = body_between(session, "void ModelAssetEditorSession::sendSourceChangeScan()", "bool ModelAssetEditorSession::reloadMeshFromSource(")
for token in (
    "scanSourceFolderMetadataInventory",
    "sourceFileFingerprint(entry.file)",
    'existing.hash == candidate.hash',
    "addSourcePart(li, candidate.entry.sourcePath, false, false)",
    "replaceSourcePart(li, gi, false, false)",
    "resetMeshStageChecks(li, geometry.id)",
    'record.stageChecks["source"] = "failed"',
    '"ambiguous_filename"',
    '"missing_source"',
    '"sourceAssetDirectory"',
    '"directoryEnumerations"',
    '"hashReads"',
):
    if token not in scan_body:
        raise AssertionError(f"0.10.58 exact-hash SOURCE synchronization missing {token!r}")
for forbidden in (
    "ModelAssetBinary::",
    "ensureAllLodsLoaded()",
    "ensureLodLoaded(",
    "prepareOneGeometry(",
    "analyzeOneGeometry(",
    "canonicalizeLoadedWorkingSet(",
    "prepareOneGeometry(",
    "analyzeOneGeometry(",
):
    if forbidden in scan_body:
        raise AssertionError(f"0.10.58 SOURCE scan leaked forbidden heavy path {forbidden!r}")

for token in (
    'runtime ? CatalogBootstrapMode::RuntimeAssembly : CatalogBootstrapMode::Folder',
    'CatalogSourceAuthority::Folder',
    'if (it->sourceAuthority == CatalogSourceAuthority::Folder && !ensureAllLodsLoaded()) return false;',
    'command == "reload_mesh_from_source"',
    "selectedSourceFilePath",
    "sendAsset({lodIndex}, true)",
    '"preserveUiSelection", preserveUiSelection',
    'out["workingSavedAtUtc"]',
    'out["workingSaveRevision"]',
    'out["sourceAssetDirectory"]',
    'out["meshSourceRecords"]',
    'out["aggregateStageChecks"]',
    '"workingFilesRoot"',
):
    if token not in session:
        raise AssertionError(f"0.10.58 source/open/persistence authority missing {token!r}")

for token in (
    "workingSaveStamp",
    "WORKING r",
    "settingsWorkingRoot",
    "meshValidationPending",
    "↻ SOURCE",
    "reload_mesh_from_source",
    "Never reads SOURCE OBJ",
    "preserveUiSelection",
    'id="geometrySection" class="section" data-wizard-groups="source"',
):
    if token not in web:
        raise AssertionError(f"0.10.58 source lifecycle UI missing {token!r}")

require(
    "tools/model_asset_editor/PATCH_CONTRACT.md",
    "exact-hash synchronization",
    "RELOAD LOD is not RELOAD FROM SOURCE",
    "SEMANTICS-only sockets",
    "exactly one WORKING save",
    "meshSourceRecords",
)

print("[PASS] model asset editor v0.10.59 source graph / exact-hash synchronization / WORKING revision")
