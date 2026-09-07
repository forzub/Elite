#!/usr/bin/env python3
"""Architecture contract for v0.10.58 SOURCE provenance/synchronization.

This test is deliberately compile-independent so it can run before the editor
binary exists. It validates the control-flow boundaries that caused the 0.10.57
regression: Cobra routing, saved source-folder identity, exact-hash comparison,
targeted add/replace, per-mesh evidence reset, eager Folder LOD residency and the
absence of .elmesh/repair work from SCAN SOURCE CHANGES.
"""
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


header = text("tools/model_asset_editor/ModelAssetEditorSession.h")
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
web = text("src/assets/webui/model_asset_editor.html")
runtime_importer = text("tools/model_asset_editor/RuntimeAssemblyImporter.cpp")
contract = text("tools/model_asset_editor/PATCH_CONTRACT.md")

# ---------------------------------------------------------------------------
# Per-mesh source graph and save-level aggregate state.
# ---------------------------------------------------------------------------
for token in (
    "struct MeshSourceRecord",
    "sourceFileName",
    "sourcePath",
    "sourceHash",
    "stageChecks",
    "meshSourceRecords",
    "m_workingSaveRevision",
    "m_loadedSourceAssetDirectory",
    "m_workingFilesRoot",
):
    if token not in header:
        raise AssertionError(f"per-mesh source graph header missing {token!r}")

for token in (
    'state["schemaVersion"] = 15',
    'state["saveRevision"] = saveRevision',
    'state["savedAtUtc"] = savedAtUtc',
    'state["sourceAssetDirectory"] = m_loadedSourceAssetDirectory.generic_string()',
    'state["aggregateStageChecks"] = aggregateStageChecksJson()',
    '"meshSourceRecords", std::move(meshSourceRecords)',
    "serializeMeshSourceRecords()",
    "aggregateStageChecksJson()",
):
    if token not in session:
        raise AssertionError(f"save/source graph serialization missing {token!r}")

# Both WORKING and final production sidecars carry schema-15 revision/check/scale data.
working_state = body_between(
    session,
    "bool ModelAssetEditorSession::writeWorkingEditorState(",
    "bool ModelAssetEditorSession::loadWorkingEditorState(",
)
production_state = body_between(
    session,
    "bool ModelAssetEditorSession::writeProductionEditorState(",
    "bool ModelAssetEditorSession::loadProductionEditorState(",
)
for label, body in (("working", working_state), ("production", production_state)):
    for token in (
        'state["schemaVersion"] = 15',
        'state["saveRevision"]',
        'state["sourceAssetDirectory"]',
        'state["stages"]',
        'state["aggregateStageChecks"]',
    ):
        if token not in body:
            raise AssertionError(f"{label} state missing {token!r}")

# One save only: revision advances inside the single editor_state.json, no history.
save_body = body_between(
    session,
    "bool ModelAssetEditorSession::saveWorkingAsset(bool quiet)",
    "bool ModelAssetEditorSession::saveAsset()",
)
for token in (
    "const std::uint64_t nextSaveRevision = m_workingSaveRevision + 1;",
    "writeWorkingEditorState(savedAtUtc, nextSaveRevision, &error)",
    "m_workingSaveRevision = nextSaveRevision;",
):
    if token not in save_body:
        raise AssertionError(f"single WORKING revision contract missing {token!r}")
for forbidden in ("save_001", "saveHistory", "checkpoint", "snapshotSequence"):
    if forbidden.lower() in save_body.lower():
        raise AssertionError(f"save history/checkpoint mechanism returned: {forbidden!r}")

# ---------------------------------------------------------------------------
# Canonical Cobra routing: Folder geometry authority + Runtime semantic bootstrap.
# ---------------------------------------------------------------------------
catalog_body = body_between(
    session,
    "ModelAssetEditorSession::ModelAssetEditorSession(",
    "std::filesystem::path ModelAssetEditorSession::compiledPath",
)
for token in (
    'std::string("Cobra Mk.I — ") + directory.filename().string()',
    "CatalogSourceAuthority::Folder",
    "runtime ? CatalogBootstrapMode::RuntimeAssembly : CatalogBootstrapMode::Folder",
):
    if token not in catalog_body:
        raise AssertionError(f"Cobra Folder/runtime split missing {token!r}")

if "i.sourceAuthority==='folder'?'SOURCE':'RUNTIME'" not in web:
    raise AssertionError("catalog SOURCE/RUNTIME marker is not driven by sourceAuthority")

for token in (
    "Runtime descriptors are semantic bootstrap, never a geometry allow-list.",
    "appendFolderOnlyLod0Meshes",
    "runtimeLod0FileKeys",
):
    if token not in runtime_importer:
        raise AssertionError(f"runtime bootstrap still suppresses folder geometry: {token!r}")

# Folder-authoritative OPEN must not leave sibling declared LODs unloaded.
select_body = body_between(
    session,
    "bool ModelAssetEditorSession::selectAsset(const std::string& id, bool forceReimport)",
    "bool ModelAssetEditorSession::saveWorkingAsset(bool quiet)",
)
if select_body.count(
    "if (it->sourceAuthority == CatalogSourceAuthority::Folder && !ensureAllLodsLoaded()) return false;"
) < 2:
    raise AssertionError("Folder OPEN must eagerly load declared WORKING and production LODs")

# ---------------------------------------------------------------------------
# Exact-hash SOURCE synchronization.
# ---------------------------------------------------------------------------
scan = body_between(
    session,
    "void ModelAssetEditorSession::sendSourceChangeScan()",
    "bool ModelAssetEditorSession::confirmSourceMeshDeletion(",
)
for token in (
    "m_loadedSourceAssetDirectory.empty()",
    "scanSourceFolderMetadataInventory(",
    "sourceFileFingerprint(entry.file)",
    "using FileKey = std::tuple<std::size_t, bool, std::string>;",
    "lowerText(candidate.fileName)",
    "existing.hash == candidate.hash && existing.hash != 0",
    "addSourcePart(li, candidate.entry.sourcePath, false, false)",
    "replaceSourcePart(li, gi, false, false)",
    "resetMeshStageChecks(li, geometry.id)",
    'record.sourceMissing = true',
    'row["kind"] = "added"',
    'row["kind"] = "replaced"',
    '"missing_source"',
    '"ambiguous_filename"',
):
    if token not in scan:
        raise AssertionError(f"exact-hash SOURCE synchronization missing {token!r}")

# Equal hash exits before any importer call for that candidate.
match_pos = scan.index("if (existing.hash == candidate.hash && existing.hash != 0)")
replace_pos = scan.index("replaceSourcePart(li, gi, false, false)", match_pos)
match_block = scan[match_pos:replace_pos]
if "continue; // unchanged hashes are intentionally omitted from visible delta rows" not in match_block:
    raise AssertionError("equal SOURCE hash no longer has a no-import fast path")
if "importObjNative(" in match_block or "replaceSourcePart(" in match_block:
    raise AssertionError("unchanged SOURCE hash reaches importer/replacement")

# Scan may parse OBJ only through add/replace helpers after comparison. Heavy
# editor/package operations are forbidden directly in the scan control flow.
for forbidden in (
    "ModelAssetBinary::",
    "ensureAllLodsLoaded()",
    "ensureLodLoaded(",
    "importObjNative(",
    "prepareOneGeometry(",
    "analyzeOneGeometry(",
    "canonicalizeLoadedWorkingSet(",
    "buildComponentCullMesh(",
):
    if forbidden in scan:
        raise AssertionError(f"SOURCE scan directly calls forbidden heavy path {forbidden!r}")

# Changed/new import helpers clear the per-mesh evidence and do not auto-save.
for function_start, function_end in (
    ("bool ModelAssetEditorSession::replaceSourcePart(", "bool ModelAssetEditorSession::replaceSourcePartByPath("),
    ("bool ModelAssetEditorSession::addSourcePart(", "bool ModelAssetEditorSession::importSourceVariantMaintenance("),
    ("bool ModelAssetEditorSession::importSourceVariantMaintenance(", "bool ModelAssetEditorSession::prepareOneGeometry("),
):
    body = body_between(session, function_start, function_end)
    if "resetMeshStageChecks(" not in body:
        raise AssertionError(f"SOURCE apply helper does not clear mesh checks: {function_start}")
    for forbidden in ("saveWorkingAsset(", "writeWorkingEditorState("):
        if forbidden in body:
            raise AssertionError(f"SOURCE apply helper auto-saves: {function_start} -> {forbidden}")

# Whole-stage CHECK leaves already-passed evidence untouched and only transitions
# pending source-mesh records. SOURCE add/replace is what clears affected graphs.
record = body_between(
    session,
    "void ModelAssetEditorSession::recordMeshStageResult(",
    "bool ModelAssetEditorSession::meshSourceRecordPending(",
)
for token in (
    'if (value == "passed")',
    'continue;',
    'if (passed)',
    'value = "passed"',
    'value = "failed"',
):
    if token not in record:
        raise AssertionError(f"granular stage evidence behavior missing {token!r}")

pending = body_between(
    session,
    "bool ModelAssetEditorSession::meshSourceRecordPending(",
    "nlohmann::json ModelAssetEditorSession::serializeMeshSourceRecords()",
)
if "for (const char* stage : wizardStageOrder())" not in pending:
    raise AssertionError("mesh pending state no longer covers every editor stage")

# ---------------------------------------------------------------------------
# Settings/status/UI visibility.
# ---------------------------------------------------------------------------
for token in (
    'id="settingsWorkingRoot"',
    "workingFilesRoot:working",
    "WORKING r${rev}",
    "meshValidationPending",
    "sourceHash",
    "source_change_scan_result",
):
    if token not in web:
        raise AssertionError(f"SOURCE graph/settings/status UI missing {token!r}")

# The bottom status path slot is intentionally repurposed to loaded revision.
status_fn = body_between(web, "function status(", "function vecInputs(")
if "WORKING r${rev}" not in status_fn:
    raise AssertionError("status bar does not show loaded WORKING revision")
if "$('ioPath').textContent=revision" not in status_fn:
    raise AssertionError("status bar path field was not replaced by save revision")

# Common stage-aware styling must be used by independent mesh lists.
if web.count("meshStageVisualClass(") < 6:
    raise AssertionError("stage-aware mesh styling is not shared across mesh lists")
for token in (
    ".meshValidationPending{background:#321b1b!important;color:#d98282!important",
    ".meshStagePassed{background:#07170f!important;color:#7fd7a0!important",
    ".meshSourceMissing{background:#351116!important;color:#ffd166!important",
    "function meshStageVisualClass",
    "renderGeometries()",
    "renderRenderTree()",
    "renderModelPreflightInventory(root)",
    "structuralGraphMeshRowsHtml()",
):
    if token not in web:
        raise AssertionError(f"stage-aware mesh list contract missing {token!r}")

# Contract itself must lock the exact boundaries so later patches cannot quietly
# reintroduce runtime geometry authority or passive/metadata-only scan behavior.
for token in (
    "exact-hash synchronization",
    "Geometry authority vs semantic bootstrap",
    "exactly one WORKING save",
    "meshSourceRecords",
    "RELOAD LOD is not RELOAD FROM SOURCE",
    "SEMANTICS-only sockets",
):
    if token not in contract:
        raise AssertionError(f"PATCH_CONTRACT missing {token!r}")

# Capability registry must expose this contract too.
cap = json.loads(text("tools/model_asset_editor/EDITOR_CAPABILITIES.json"))
if "source_hash_mesh_graph" not in {x["id"] for x in cap["protected_capabilities"]}:
    raise AssertionError("source_hash_mesh_graph capability is not protected")

require("tools/model_asset_editor/EditorVersion.h", 'ModelAssetEditorVersion = "0.10.64"')
print("[PASS] v0.10.64 exact-hash SOURCE synchronization / per-mesh source graph")
