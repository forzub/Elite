#!/usr/bin/env python3
"""Architecture contract for v0.10.62 stage certification / confirmed SOURCE deletion.

This guard protects the exact behavior requested for SOURCE maintenance:
CHECK updates per-mesh stage evidence immediately, unchanged hashes keep their
prior evidence, changed/new meshes clear only their own evidence, and missing
files become a persisted two-phase deletion state that requires explicit user
confirmation before resident geometry is removed.
"""
from pathlib import Path

from model_asset_editor_source_bundle import load_source_bundle
import json

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def between(body: str, start: str, end: str) -> str:
    a = body.index(start)
    b = body.index(end, a + len(start))
    return body[a:b]


header = text("tools/model_asset_editor/ModelAssetEditorSession.h")
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
web = load_source_bundle(ROOT)
contract = text("tools/model_asset_editor/PATCH_CONTRACT.md")
locale = json.loads(text("src/assets/localization/ui/tools/model_asset_editor.json"))
capabilities = json.loads(text("tools/model_asset_editor/EDITOR_CAPABILITIES.json"))

# Persisted graph owns a separate missing-file state; absence is not encoded by
# destroying old per-stage evidence.
for token in (
    "struct MeshSourceRecord",
    "bool sourceMissing = false;",
    "std::map<std::string, std::string> stageChecks;",
    "bool confirmSourceMeshDeletion(std::size_t lodIndex, const std::string& geometryId);",
):
    if token not in header:
        raise AssertionError(f"missing deletion/source-graph header contract: {token!r}")

for token in (
    'state["schemaVersion"] = 17',
    '{"sourceHash", record.sourceHash}, {"sourceMissing", record.sourceMissing}',
    'record.sourceMissing = schemaVersion >= 14 && item.value("sourceMissing", false);',
    '{"sourceMissing", haveSourceRecord && sourceRecord.sourceMissing}',
):
    if token not in session:
        raise AssertionError(f"schema-17 missing state not persisted/exposed: {token!r}")

# CHECK is still persistence-read-only but must push new per-mesh evidence to UI
# immediately. A successful whole-stage check certifies all current meshes;
# failed re-check preserves already-passed evidence for unaffected meshes.
check = between(
    session,
    "bool ModelAssetEditorSession::checkWizardStage(const std::string& stage)",
    "bool ModelAssetEditorSession::scanRenderDuplicates(",
)
for token in (
    "recordMeshStageResult(stage, passed, stage != \"build\");",
    "sendAssetMetadata();",
    '"type", "wizard_state_patch"',
):
    if token not in check:
        raise AssertionError(f"CHECK does not publish per-mesh stage state: {token!r}")
for forbidden in ("saveWorkingAsset(", "writeWorkingEditorState("):
    # BUILD may call a production-sidecar write, but CHECK must not invoke the
    # mutable WORKING SAVE path itself.
    if forbidden in check:
        raise AssertionError(f"CHECK unexpectedly persists WORKING state: {forbidden!r}")

record = between(
    session,
    "void ModelAssetEditorSession::recordMeshStageResult(",
    "bool ModelAssetEditorSession::meshSourceRecordPending(",
)
for token in (
    'if (value == "passed")',
    'continue;',
    'if (passed)',
    'value = "passed";',
    'value = "failed";',
):
    if token not in record:
        raise AssertionError(f"per-mesh stage certification behavior missing {token!r}")

# SCAN: missing source only marks the two-phase state. It must not erase the
# geometry, stage evidence, save the asset, or call the confirmation mutation.
scan = between(
    session,
    "void ModelAssetEditorSession::sendSourceChangeScan()",
    "bool ModelAssetEditorSession::confirmSourceMeshDeletion(",
)
for token in (
    "existingRecord.sourceMissing = false;",
    "record.sourceMissing = false;",
    "record.sourceMissing = true;",
    '"kind", "missing_source"',
    "existing.hash == candidate.hash && existing.hash != 0",
    "resetMeshStageChecks(li, geometry.id)",
):
    if token not in scan:
        raise AssertionError(f"SOURCE scan missing two-phase state behavior: {token!r}")
for forbidden in (
    "lod.geometries.erase",
    "lod.nodes.erase",
    "confirmSourceMeshDeletion(",
    "saveWorkingAsset(",
    "writeWorkingEditorState(",
    'record.stageChecks["source"] = "failed"',
):
    if forbidden in scan:
        raise AssertionError(f"SCAN performs forbidden automatic deletion/evidence rewrite: {forbidden!r}")

# Confirm is the only destructive missing-file route. It rechecks SOURCE,
# removes all render instances and geometry, preserves surviving child world
# transforms, then clears geometry-owned editor/source/variant metadata.
confirm = between(
    session,
    "bool ModelAssetEditorSession::confirmSourceMeshDeletion(",
    "bool ModelAssetEditorSession::reloadMeshFromSource(",
)
for token in (
    "!sourceIt->second.sourceMissing",
    "selectedSourceFilePath(sourcePath, &sourceError)",
    "SOURCE deletion refused: the file exists again",
    "removedNodes.insert(ri)",
    "oldWorld",
    "composeRigid(inverseRigid(parentWorld), oldWorld[oldIndex])",
    "lod.geometries.erase",
    "m_meshPreparationRecords[lodIndex].erase(geometryId)",
    "m_meshOrientationOverrides[lodIndex].erase(geometryId)",
    "m_geometryTopologyClasses[lodIndex].erase(geometryId)",
    "m_rawMeshSnapshots[lodIndex].erase(geometryId)",
    "m_sourceMeshFingerprints[lodIndex].erase(sourcePath)",
    "m_sourceMeshQuickStamps[lodIndex].erase(sourcePath)",
    "m_meshSourceRecords[lodIndex].erase(geometryId)",
    "m_sourceExtraMeshIds[lodIndex].erase(sourcePath)",
    "m_baseVisualIds[lodIndex].erase(geometryId)",
    "markLodDirty(lodIndex)",
    'invalidateWizardFrom("source")',
    '"type", "source_mesh_deletion_confirmed"',
):
    if token not in confirm:
        raise AssertionError(f"confirmed SOURCE deletion incomplete: {token!r}")
for forbidden in (
    "m_asset.nodes.erase",
    "semanticNodes.erase",
    "m_asset.sockets.erase",
    "saveWorkingAsset(",
):
    if forbidden in confirm:
        raise AssertionError(f"confirmed mesh deletion destroys cross-LOD semantics or auto-saves: {forbidden!r}")

handle = between(session, "void ModelAssetEditorSession::handleMessage(", "} // namespace elite::model_asset::editor")
for token in ('command == "confirm_source_mesh_deletion"', "confirmSourceMeshDeletion("):
    if token not in handle:
        raise AssertionError(f"deletion confirmation command not routed: {token!r}")

# UI state is stage-aware: passed current stage = green, unverified = red-brown,
# missing source = yellow/dark-red with explicit confirmation.
for token in (
    ".meshStagePassed{background:#07170f!important;color:#7fd7a0!important",
    ".meshValidationPending{background:#321b1b!important;color:#d98282!important",
    ".meshSourceMissing{background:#351116!important;color:#ffd166!important",
    "function meshStageVisualClass",
    "if(g?.sourceMissing)return 'missing'",
    "value==='passed'?'meshStagePassed':'meshValidationPending'",
    "sourceDeleteConfirm",
    "data-confirm-source-delete",
    "confirmMissingSourceMesh",
    "send('confirm_source_mesh_deletion'",
    "source_mesh_deletion_confirmed",
):
    if token not in web:
        raise AssertionError(f"stage/deleted mesh UI contract missing {token!r}")

# Localization must expose the exact operator-visible deletion state/action.
strings = locale["strings"]
if strings["model_editor.source.deleted"]["ru"] != "УДАЛЕН":
    raise AssertionError("Russian deleted marker changed")
if strings["model_editor.source.confirm_delete"]["ru"] != "ПОДТВЕРДИТЬ":
    raise AssertionError("Russian confirm action changed")

for token in (
    "Per-mesh stage certification and two-phase SOURCE deletion",
    "SCAN **must not delete it**",
    "confirm_source_mesh_deletion",
    "green on a dark green-black background",
):
    if token not in contract:
        raise AssertionError(f"PATCH_CONTRACT missing deletion/check rule {token!r}")

caps = {item["id"]: item for item in capabilities["protected_capabilities"]}
cap = caps.get("source_hash_mesh_graph")
if not cap or "sourceMissing" not in str(cap) or "confirm_source_mesh_deletion" not in str(cap):
    raise AssertionError("source_hash_mesh_graph capability does not protect two-phase deletion")

if 'ModelAssetEditorVersion = "0.10.73"' not in text("tools/model_asset_editor/EditorVersion.h"):
    raise AssertionError("editor version is not 0.10.73")

print("[PASS] model asset editor v0.10.73 stage certification / confirmed SOURCE deletion")
