#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def require(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing {token!r}")

require(
    "tools/model_asset_editor/SourceFolderImporter.h",
    "struct SourceFolderMesh",
    "discoverSourceFolderOrdinaryMeshes",
)
require(
    "tools/model_asset_editor/SourceFolderImporter.cpp",
    "discoverSourceFolderOrdinaryMeshes",
    "directObjFiles",
    "sourcePathFor",
)

session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
header = text("tools/model_asset_editor/ModelAssetEditorSession.h")
for token in (
    "sourceMeshFingerprints",
    "componentMaintenanceIssues",
    "sendSourceChangeScan",
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

for command in (
    'command == "scan_source_changes"',
    'command == "adopt_source_revision"',
    'command == "adopt_all_source_revisions"',
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

replace_start = session.index("bool ModelAssetEditorSession::replaceSourcePart(")
replace_end = session.index("bool ModelAssetEditorSession::addSourcePart(", replace_start)
replace_body = session[replace_start:replace_end]
for token in (
    "geometry.mesh = std::move(mesh)",
    'markMaintenanceIssues(componentId, {"prepare", "surfaces"})',
    "m_sourceMeshFingerprints",
    "markLodDirty(lodIndex)",
):
    if token not in replace_body:
        raise AssertionError(f"part replacement lost local-preservation contract {token!r}")
for forbidden in (
    'invalidateWizardFrom("source")',
    'invalidateWizardFrom("geometry")',
    'm_asset.nodes.clear()',
    'm_asset.collisionVolumes.clear()',
):
    if forbidden in replace_body:
        raise AssertionError(f"part replacement still destroys unrelated authored work: {forbidden!r}")

add_start = session.index("bool ModelAssetEditorSession::addSourcePart(")
add_end = session.index("bool ModelAssetEditorSession::importSourceVariantMaintenance(", add_start)
add_body = session[add_start:add_end]
for token in (
    "RenderGeometryDefinition geometry",
    "RenderNode node",
    "node.semanticNodeIndex = NoIndex",
    'markMaintenanceIssues(componentId, {"prepare", "surfaces", "semantics"})',
):
    if token not in add_body:
        raise AssertionError(f"new-part maintenance contract missing {token!r}")
if 'invalidateWizardFrom(' in add_body:
    raise AssertionError("adding one source part still globally invalidates the wizard")

variant_start = session.index("bool ModelAssetEditorSession::importSourceVariantMaintenance(")
variant_end = session.index("bool ModelAssetEditorSession::prepareOneGeometry(", variant_start)
variant_body = session[variant_start:variant_end]
for token in (
    "makeRenderVariantGeometryId",
    "allocateSourceVariantId",
    'markMaintenanceIssues(componentId, {"prepare", "surfaces"})',
    'markMaintenanceIssues(componentId, {"replacement"})',
):
    if token not in variant_body:
        raise AssertionError(f"variant maintenance contract missing {token!r}")
if "RenderNode" in variant_body:
    raise AssertionError("additional maintenance variant unexpectedly creates a normal RenderNode")

regen_start = session.index("bool ModelAssetEditorSession::regenerateDerivedLodsForGeometry(")
regen_end = session.index("nlohmann::json ModelAssetEditorSession::serializeAssetMetadata", regen_start)
regen_body = session[regen_start:regen_end]
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
if 'invalidateWizardFrom(' in regen_body:
    raise AssertionError("selected derived-LOD regeneration still globally invalidates the wizard")

for token in (
    '"maintenance"',
    '"maintenanceComponentId"',
    '"maintenanceIssues"',
    '"source_change_scan_result"',
    '"geometry_preflight_result"',
):
    if token not in session:
        raise AssertionError(f"maintenance metadata/reporting missing {token!r}")


# Maintenance V2: legacy production assets get one baseline migration, and
# normal scans emit only delta rows.
scan_start = session.index("void ModelAssetEditorSession::sendSourceChangeScan()")
scan_end = session.index("bool ModelAssetEditorSession::replaceSourcePart(", scan_start)
scan_body = session[scan_start:scan_end]
for token in (
    '"baselineRequired"',
    '"baselineMissing"',
    'current files never enter the delta table',
    'if (accepted == currentFingerprint)',
):
    if token not in scan_body:
        raise AssertionError(f"maintenance V2 delta scan missing {token!r}")
for forbidden in ('{"kind", "untracked"}', '{"kind", "variant_untracked"}'):
    if forbidden in scan_body:
        raise AssertionError(f"maintenance V2 still emits per-file baseline spam: {forbidden!r}")
if "adoptAllSourceRevisions" not in session or "writeWorkingEditorState(&error)" not in session[session.index("bool ModelAssetEditorSession::adoptAllSourceRevisions()"):scan_start]:
    raise AssertionError("one-shot source baseline migration is not persisted")

# VALIDATE/BUILD must refuse to publish a package with unresolved local debt.
require(
    "tools/model_asset_editor/ModelAssetEditorSession.cpp",
    "blocked by pending maintenance",
    '"stage", "maintenance"',
)

web = text("src/assets/webui/model_asset_editor.html")
for token in (
    "partMaintenanceBlock",
    "renderPartMaintenance",
    "maintenanceSourceScanHtml",
    "bindMaintenanceSourceScan",
    "maintenanceScopeWhole",
    "maintenanceScopeSelected",
    "maintenanceWorksetBarHtml",
    "maintenanceLodWorkHtml",
    "data-maintenance-adopt-all",
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
):
    if token not in web:
        raise AssertionError(f"maintenance UI missing {token!r}")


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
    raise AssertionError("SOURCE change scan must be visible before destructive refresh/reimport controls")

handle_start = web.index("function handle(msg)")
handle_end = web.index("function connect()", handle_start)
handle_body = web[handle_start:handle_end]
for token in (
    "source_change_scan_result",
    "state.wizardStage==='source'",
    "renderWizardPanel()",
    "renderPartMaintenance()",
):
    if token not in handle_body:
        raise AssertionError(f"SOURCE scan result routing missing {token!r}")

require(
    "tools/model_asset_editor/EditorVersion.h",
    'ModelAssetEditorVersion = "0.10.32"',
)

print("[PASS] model asset editor v0.10.32 maintenance work set / delta-only SOURCE baseline / persistent working state")
