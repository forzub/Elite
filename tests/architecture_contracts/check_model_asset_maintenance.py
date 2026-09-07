#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def require(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing {token!r}")


def between(body: str, start: str, end: str) -> str:
    a = body.index(start)
    b = body.index(end, a + len(start))
    return body[a:b]


require(
    "tools/model_asset_editor/SourceFolderImporter.h",
    "struct SourceFolderMesh",
    "struct SourceFolderMetadataInventory",
    "discoverSourceFolderOrdinaryMeshes",
    "scanSourceFolderMetadataInventory",
)
require(
    "tools/model_asset_editor/SourceFolderImporter.cpp",
    "discoverSourceFolderOrdinaryMeshes",
    "directObjFiles",
    "sourcePathFor",
    "scanSourceFolderMetadataInventory",
)

session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
header = text("tools/model_asset_editor/ModelAssetEditorSession.h")
web = text("src/assets/webui/model_asset_editor.html")

for token in (
    "sourceMeshFingerprints",
    "meshSourceRecords",
    "componentMaintenanceIssues",
    "sendSourceChangeScan",
    "reloadMeshFromSource",
    "replaceSourcePart",
    "addSourcePart",
    "importSourceVariantMaintenance",
    "prepareOneGeometry",
    "analyzeOneGeometry",
    "regenerateDerivedLodsForGeometry",
    "maintenanceComponentId",
):
    if token not in session and token not in header:
        raise AssertionError(f"maintenance backend missing {token!r}")

# Retired baseline/adopt commands must stay gone. Hash change is applied by scan;
# there is no longer an action that accepts a new source revision without geometry.
for command in (
    'command == "scan_source_changes"',
    'command == "reload_mesh_from_source"',
    'command == "replace_source_part"',
    'command == "add_source_part"',
    'command == "replace_source_variant"',
    'command == "add_source_variant"',
    'command == "prepare_geometry"',
    'command == "analyze_geometry_preflight"',
    'command == "regenerate_geometry_lods"',
):
    if command not in session:
        raise AssertionError(f"maintenance command missing {command!r}")
for forbidden in (
    'command == "adopt_source_revision"',
    'command == "adopt_all_source_revisions"',
    "adoptSourceRevision",
    "adoptAllSourceRevisions",
):
    if forbidden in session or forbidden in header:
        raise AssertionError(f"unsafe source-baseline acceptance path returned: {forbidden!r}")

replace_body = between(
    session,
    "bool ModelAssetEditorSession::replaceSourcePart(",
    "bool ModelAssetEditorSession::replaceSourcePartByPath(",
)
for token in (
    "geometry.mesh = std::move(mesh)",
    'markMaintenanceIssues(componentId, {"prepare", "surfaces"})',
    "m_sourceMeshFingerprints",
    "resetMeshStageChecks(lodIndex, geometry.id)",
    "markLodDirty(lodIndex)",
):
    if token not in replace_body:
        raise AssertionError(f"part replacement lost local-preservation contract {token!r}")
for forbidden in (
    'invalidateWizardFrom("source")',
    'invalidateWizardFrom("geometry")',
    "m_asset.nodes.clear()",
    "m_asset.collisionVolumes.clear()",
    "saveWorkingAsset(",
):
    if forbidden in replace_body:
        raise AssertionError(f"part replacement still destroys/saves unrelated work: {forbidden!r}")

add_body = between(
    session,
    "bool ModelAssetEditorSession::addSourcePart(",
    "bool ModelAssetEditorSession::importSourceVariantMaintenance(",
)
for token in (
    "RenderGeometryDefinition geometry",
    "RenderNode node",
    "node.semanticNodeIndex = NoIndex",
    'markMaintenanceIssues(componentId, {"prepare", "surfaces", "semantics"})',
    "resetMeshStageChecks(lodIndex, resident.id)",
):
    if token not in add_body:
        raise AssertionError(f"new-part maintenance contract missing {token!r}")
if "saveWorkingAsset(" in add_body:
    raise AssertionError("adding one source part still saves implicitly")

variant_body = between(
    session,
    "bool ModelAssetEditorSession::importSourceVariantMaintenance(",
    "bool ModelAssetEditorSession::prepareOneGeometry(",
)
for token in (
    "makeRenderVariantGeometryId",
    "allocateSourceVariantId",
    'markMaintenanceIssues(componentId, {"prepare", "surfaces"})',
    'markMaintenanceIssues(componentId, {"replacement"})',
    "resetMeshStageChecks(lodIndex, resident.id)",
):
    if token not in variant_body:
        raise AssertionError(f"variant maintenance contract missing {token!r}")
if "RenderNode node" in variant_body:
    raise AssertionError("additional maintenance variant unexpectedly creates a normal RenderNode")

regen_body = between(
    session,
    "bool ModelAssetEditorSession::regenerateDerivedLodsForGeometry(",
    "nlohmann::json ModelAssetEditorSession::serializeAssetMetadata",
)
for token in (
    "buildComponentCullMesh",
    'targetLod.sourceKind != "generated"',
    "manualSkipped",
    "sourceIsVariant",
    "m_baseVisualIds[li]",
    "m_sourceExtraMeshIds[li]",
):
    if token not in regen_body:
        raise AssertionError(f"selected derived-LOD maintenance missing {token!r}")
if "invalidateWizardFrom(" in regen_body:
    raise AssertionError("selected derived-LOD regeneration still globally invalidates wizard")

# v0.10.58 scan: one source inventory, exact hash, automatic add/replace, no
# passive ADOPT action and no package/repair work.
scan = between(
    session,
    "void ModelAssetEditorSession::sendSourceChangeScan()",
    "bool ModelAssetEditorSession::confirmSourceMeshDeletion(",
)
for token in (
    "scanSourceFolderMetadataInventory",
    "sourceFileFingerprint(entry.file)",
    "existing.hash == candidate.hash",
    "addSourcePart(li, candidate.entry.sourcePath, false, false)",
    "replaceSourcePart(li, gi, false, false)",
    "resetMeshStageChecks(li, geometry.id)",
    'record.sourceMissing = true',
    '"sourceAssetDirectory"',
    '"hashReads"',
    '"elapsedMs"',
):
    if token not in scan:
        raise AssertionError(f"exact-hash maintenance scan missing {token!r}")
for forbidden in (
    "ModelAssetBinary::",
    "ensureAllLodsLoaded()",
    "ensureLodLoaded(",
    "importObjNative(",
    "prepareOneGeometry(",
    "analyzeOneGeometry(",
    "canonicalizeLoadedWorkingSet(",
    "writeWorkingEditorState(",
    "saveWorkingAsset(",
):
    if forbidden in scan:
        raise AssertionError(f"SOURCE scan leaked forbidden heavy/persistence work: {forbidden!r}")

# VALIDATE/BUILD still refuse unresolved maintenance debt.
require(
    "tools/model_asset_editor/ModelAssetEditorSession.cpp",
    "blocked by pending maintenance",
    '"stage", "maintenance"',
)

for token in (
    "maintenanceSourceScanHtml",
    "bindMaintenanceSourceScan",
    "maintenanceScopeWhole",
    "maintenanceScopeSelected",
    "maintenanceWorksetBarHtml",
    "maintenanceLodWorkHtml",
    "maintenanceFileName",
    "data-maintenance-scan-source",
    "replace_source_part",
    "add_source_part",
    "replace_source_variant",
    "add_source_variant",
    "prepare_geometry",
    "analyze_geometry_preflight",
    "regenerate_geometry_lods",
    "source_change_scan_result",
    "reload_mesh_from_source",
    "↻ SOURCE",
    "meshValidationPending",
):
    if token not in web:
        raise AssertionError(f"maintenance UI missing {token!r}")
for forbidden in ("data-maintenance-adopt-all", "ADOPT SOURCE REVISION", "ACCEPT BASELINE"):
    if forbidden in web:
        raise AssertionError(f"unsafe/passive source adoption UI returned: {forbidden!r}")

source_stage = web[web.index("if(stage==='source')"):web.index("if(stage==='lods')")]
for token in (
    "maintenanceSourceScanHtml()",
    "bindMaintenanceSourceScan(root)",
    "wizardSourceRefreshBtn",
    "wizardSourceReimportBtn",
):
    if token not in source_stage:
        raise AssertionError(f"SOURCE stage maintenance scan missing {token!r}")
if source_stage.index("maintenanceSourceScanHtml()") > source_stage.index("wizardSourceRefreshBtn"):
    raise AssertionError("SOURCE change scan must remain visible before whole-asset reimport controls")

handle = between(web, "function handle(msg)", "function connect()")
for token in (
    "source_change_scan_result",
    "state.wizardStage==='source'",
    "renderWizardPanel()",
    "renderPartMaintenance()",
):
    if token not in handle:
        raise AssertionError(f"SOURCE scan result routing missing {token!r}")

require(
    "tools/model_asset_editor/EditorVersion.h",
    'ModelAssetEditorVersion = "0.10.63"',
)

print("[PASS] model asset editor v0.10.63 maintenance / exact-hash SOURCE apply / manual SAVE")
