from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def require(path: str, *tokens: str) -> None:
    data = text(path)
    for token in tokens:
        if token not in data:
            raise AssertionError(f"{path} missing {token!r}")


# Shared model-asset data stays renderer/runtime agnostic.
model = text("src/model_asset/ModelAsset.h")
for forbidden in ("glad/", "GLFW", "MeshGPU", "SceneRenderer", "SpaceState"):
    if forbidden in model:
        raise AssertionError(f"shared ModelAsset leaked runtime/render dependency {forbidden!r}")

require(
    "src/model_asset/ModelAsset.h",
    "ModelAssetFormatVersion = 4",
    "struct SourceBasis",
    "struct MaterialDefinition",
    "struct RenderGeometryDefinition",
    "struct RenderNode",
    "struct RenderLod",
    "declaredGeometryCount",
    "declaredNodeCount",
    "struct StateVariant",
    "defaultStateId",
    "transformOverride",
    "physicsOverride",
    "detached",
    "activeStates",
    "struct HitRegion",
    "struct Opening",
    "traversable",
    "lineOfFire",
    "struct RepairTarget",
    "repairedStateId",
    "std::vector<RenderLod> renderLods",
    "Migration-only v2/v3",
)

require(
    "src/model_asset/ModelAssetIdentity.h",
    "allocateStableId",
    "allocateChildStableId",
)

require(
    "src/model_asset/ModelAssetMigration.cpp",
    "buildIndependentRenderLodsFromLegacy",
    "legacyRenderLodCount",
    "RenderGeometryDefinition",
    "RenderNode",
    "semanticNodeIndex",
    "node.geometryIndex = NoIndex",
    "allocateStableId",
    "semanticNodeIds",
)

require(
    "src/model_asset/ModelAssetBinary.cpp",
    "ManifestMagicV4",
    "ManifestMagicV3",
    "LegacyMagicV2",
    "MeshMagicV4",
    "MeshMagicV2",
    "MeshPayloadFormatVersion = 4",
    "{'S','E','M','N'}",
    "{'S','T','A','T'}",
    "{'H','I','T','R'}",
    "{'O','P','E','N'}",
    "{'R','E','P','R'}",
    "{'L','O','D','S'}",
    "writeLodPayload",
    "validateRenderLod",
    "validateSemanticAsset",
    "ModelAssetBinary::validate",
    "duplicate RenderNode id",
    "duplicate semantic Node id",
    "saveManifest",
    "loadManifest",
    "saveLod",
    "loadLod",
    "declaredGeometryCount",
    "declaredNodeCount",
    "buildIndependentRenderLodsFromLegacy",
)

# Source import still bypasses runtime ObjLoader so topology/material authoring
# data survives. It may initially produce legacy shared geometry, which is then
# migrated to one independent render graph per LOD.
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
        raise AssertionError(f"editor importer still depends on old runtime mesh processing {forbidden!r}")
require(
    "tools/model_asset_editor/RuntimeAssemblyImporter.cpp",
    "ObjectAssemblyRegistry::get",
    "importObjNative",
    "geometryBySource",
    "moduleNodeById",
    "ShipAttachmentPoint",
    "JointType::Revolute",
    "ImportProgressCallback",
    "allocateChildStableId",
    '"mesh"',
    "meshNodeId",
)

require(
    "tools/model_asset_editor/ModelAssetEditorSession.cpp",
    "buildIndependentRenderLodsFromLegacy",
    "set_node_default_state",
    "add_state_variant",
    "set_state_variant",
    "delete_state_variant",
    "set_render_node_transform",
    "set_render_node_geometry",
    "set_render_node_semantic",
    "set_render_node_states",
    "fit_render_node_as_instance",
    "duplicate_render_node_instance",
    "break_render_node_instance",
    "create_radial_render_instances",
    "delete_render_node",
    "add_hit_region",
    "set_hit_region",
    "add_opening",
    "set_opening",
    "add_repair_target",
    "set_repair_target",
    "delete_unused_geometries",
    "renderLods",
    "activeStates",
    "source OBJ/assembly",
    "saveManifestOnly",
    "saveLodOnly",
    "loadLodData",
    "loadLodOnly",
    "unloadLod",
    "complete_wizard_stage",
    "restore_wizard_checkpoint",
    "scan_render_duplicates",
    "consolidate_render_duplicates",
    "wizardCheckpointPath",
    "wizardCheckpointEditorStatePath",
    "latestSavedWizardCheckpoint",
    "pruneWizardCheckpointsAfter",
    "restoreWizardValidityAt",
    "sendAssetMetadata",
    "serializeAssetMetadata",
    "asset_binary_begin",
    "lod_payload_binary_begin",
    "ModelAssetEditorVersion",
)

web = text("src/assets/webui/model_asset_editor.html")
for forbidden in (
    "set_node_geometry",
    "fit_node_as_instance",
    "duplicate_node_instance",
    "break_node_instance",
    "create_radial_instances",
    "state.asset.geometries",
    "SHARED IDs",
):
    if forbidden in web:
        raise AssertionError(f"v4 editor UI retained cross-LOD/shared-geometry concept {forbidden!r}")
require(
    "src/assets/webui/model_asset_editor.html",
    "Semantic assembly",
    "Part states",
    "Render LOD files",
    "Render assembly",
    "Active LOD geometry",
    "Selected semantic node",
    "Selected render node",
    "Damage / repair semantics",
    "renderLods",
    "selectedRenderNode",
    "previewStates",
    "activeStates",
    "renderRenderNodeInspector",
    "renderDamageSemantics",
    "fit_render_node_as_instance",
    "set_render_node_states",
    "set_node_default_state",
    "add_state_variant",
    "add_opening",
    "add_repair_target",
    "Every LOD is an independent render document",
    "wizardBar",
    "wizardPanel",
    "wizardGeometryScanBtn",
    "wizardGeometryCandidates",
    "geometryReference",
    "compareMatch",
    "compareNoMatch",
    "radialModal",
    "complete_wizard_stage",
    "restore_wizard_checkpoint",
    "scan_render_duplicates",
    "asset_metadata",
    "mergeAssetMetadata",
    "geometryCacheKey",
    "ELWIR001",
    "handleEditorBinary",
    "binaryType='arraybuffer'",
    "rebuildScene(true)",
    "ioProgressOverlay",
    "ioPathSep",
    "umbrellaSpinner",
    "Ctrl+Alt+F12",
    "model_asset_editor_i18n.json",
)


if "quitBtn" in web or "quit_editor" in web:
    raise AssertionError("redundant in-page quit path returned to the editor UI")
for token in ("settingsSaveTimedOut", "settingsSaveTimer", "model_editor.settings.save_timeout"):
    if token not in web:
        raise AssertionError(f"settings-save acknowledgement guard missing {token!r}")

require(
    "tools/model_asset_editor/GeometryInstanceFitter.cpp",
    "sameIndexedTopology",
    "coarsePointCloudFit",
    "principalFrame",
    "validateBidirectionalPointCloud",
    "fitGeometryAsRigidInstance",
)

require(
    "src/assets/localization/ui/tools/model_asset_editor.json",
    '"kind": "ui_strings"',
    '"model_editor.v4.section.semantic"',
    '"model_editor.v4.section.states"',
    '"model_editor.v4.section.render_assembly"',
    '"model_editor.v4.section.damage"',
    '"model_editor.v4.command.add_opening"',
    '"model_editor.v4.command.add_repair"',
    '"model_editor.wizard.source"',
    '"model_editor.wizard.lods"',
    '"model_editor.wizard.geometry"',
    '"model_editor.wizard.geometry.scan"',
    '"ru"',
    '"zh-Hans"',
    '"es"',
    '"ja"',
)

require(
    "src/model_asset/MODEL_ASSET_PIPELINE.md",
    "semantic assembly + independent render documents",
    "Semantic damage states and live structural substitution",
    "Binary format v4",
    "State is independent of render LOD",
    "runtime must apply a semantic state transition atomically",
    "Source OBJ/assembly files are read-only",
)

require("tools/model_asset_editor/EditorVersion.h", 'ModelAssetEditorVersion = "0.10.25"')
require(
    "tools/model_asset_editor/CHANGELOG.md",
    "0.10.25",
    "metadata-only surface edits / checkpoint-save independence",
    "0.10.24",
    "linear checkpoint resume / binary geometry transport",
    "0.10.23",
    "executable-owned UI package isolation",
    "0.10.22",
    "explicit SURFACES analysis / cross-LOD surface intent",
    "0.10.21",
    "SURFACES authoring workspace",
    "0.10.20",
    "restored GEOMETRY authoring workspace",
    "0.10.19",
    "LOD apply feedback / on-demand viewport payloads",
    "0.10.18",
    "LOD gate split / stable editor artifact layout",
    "0.10.16",
    "production libigl + Embree canonical preparation",
    "0.10.15",
    "minimal macro-patch mesh repair loop",
    "0.10.13",
    "shared runtime mesh normalization / analysis split",
    "0.10.12",
    "explicit mesh preparation / non-blocking load",
    "0.10.11",
    "topology-aware canonical weld fixes false station non-manifold",
    "0.10.9",
    "canonical SOURCE boundary / classification-only Preflight",
    "0.10.8",
    "canonical mesh builder / explicit preparation contract",
    "0.10.7",
    "runtime-equivalent canonical mesh preparation",
    "0.10.5",
    "SOURCE owns complete authoring set / actionable preflight / Russian UI sweep",
    "0.10.4",
    "model preflight / topology intent / safe normals repair",
    "0.10.3",
    "diagnostic LOD preview / coplanar region collapse",
    "0.10.2",
    "explicit LOD0 comparison row",
    "0.10.0",
    "optional LOD analysis / disconnected-detail preview",
    "0.9.3",
    "metadata-only UI sync / linear checkpoint pruning",
    "0.9.9",
    "resume latest wizard checkpoint",
    "0.9.8",
    "Geometry finish pass",
    "0.9.7",
    "stable extra-mesh assignment",
    "0.9.5",
    "flat source variants / explicit replacement compatibility",
    "0.9.4",
    "source render variants / XYZ orientation",
    "0.9.1",
    "stable ID preflight / station import repair",
    "0.9.0",
    "wizard pipeline / capability gate / LOD manager",
    "0.8.2",
    "reliable settings save / native window close",
    "0.8.1",
    "source-root defaults / settings acknowledgement",
    "0.8.0",
    "semantic states / independent render LOD graphs",
    "Opening",
    "RepairTarget",
)

require(
    "src/model_asset/ModelAsset.h",
    "EdgeNonManifold",
    "EdgeCanonicalTopology",
)
require(
    "tools/model_asset_editor/NativeObjImporter.cpp",
    "sourceBoundaryUseCount",
    "EdgeNonManifold",
)
require(
    "src/model_asset/RuntimeMeshNormalizer.h",
    "RuntimeMeshWeldEpsilon",
    "RuntimeMeshNormalizerAlgorithmId",
    "runtime_mesh_normalizer_v1",
    "normalizeRuntimeMeshTopology",
)
require(
    "src/model_asset/RuntimeMeshNormalizer.cpp",
    "std::unordered_map<QuantizedPosition",
    "removedDegenerateTriangles",
    "removedDuplicateTriangles",
    "pointForInputVertex",
)
require(
    "tools/model_asset_editor/CanonicalMeshBuilder.h",
    "CanonicalMeshAlgorithmId",
    "canonical_mesh_libigl_embree_v1",
    "CanonicalMeshAnalysis",
    "CanonicalMeshBuildResult",
    "canonicalizeMesh",
    "canonicalMeshFingerprint",
)
require(
    "tools/model_asset_editor/CanonicalMeshBuilder.cpp",
    "buildTopologicalPointMap",
    "repairTopologyAndOrientationWithLibigl",
    "igl::split_nonmanifold",
    "igl::embree::reorient_facets_raycast",
    "rebuildRenderVertices",
    "EdgeCanonicalTopology",
    "mesh = std::move(candidate)",
)
require(
    "src/game/geometry/ObjLoader.cpp",
    "normalizeRuntimeMeshTopology",
    "RuntimeMeshWeldEpsilon",
    "RUNTIME NORMALIZATION FAILED",
)
require(
    "CMakeLists.txt",
    "src/model_asset/RuntimeMeshNormalizer.cpp",
    "EliteModelAsset",
    "ELITE_MODEL_ASSET_LIBIGL_SPIKE",
    "igl::core",
    "igl::embree",
)
canonical_builder = text("tools/model_asset_editor/CanonicalMeshBuilder.cpp")
if "MaxStabilizationPasses" in canonical_builder:
    raise AssertionError("mesh preparation regressed to multi-pass fixed-point canonicalization")
if "const MeshLod original = mesh" in canonical_builder:
    raise AssertionError("mesh preparation restored a full deep copy of the input MeshLod")
require(
    "src/assets/webui/model_asset_editor.html",
    "modelPreflightPrepareBtn",
    "modelPreflightCheckBtn",
    "prepare_model_meshes",
    "model_editor.preflight.canonical_note",
    "ПОДГОТОВИТЬ МЕШИ",
)
for forbidden in (
    "modelPreflightRuntimeBtn",
    "runtimeNormalizedThreeGeometry",
    "apply_mesh_preparation",
    "safe_fix_model_preflight",
):
    if forbidden in web:
        raise AssertionError(f"obsolete Preflight UI returned: {forbidden!r}")

session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
for forbidden in ("safeFixModelPreflight", '"apply_mesh_preparation"', '"safe_fix_model_preflight"'):
    if forbidden in session:
        raise AssertionError(f"obsolete canonicalization backend path returned: {forbidden!r}")
for token in (
    "canonicalizeLoadedWorkingSet",
    "verifyLoadedWorkingSetCanonical",
    'command == "prepare_model_meshes"',
    "MESH PREPARATION INCOMPLETE",
    "appendMeshRepairDiagnostic",
    "wizardLogPath",
    'wizardLogPath("mesh_repair.log")',
):
    if token not in session:
        raise AssertionError(f"explicit mesh-preparation contract missing {token!r}")

canonical_start = session.index("bool ModelAssetEditorSession::canonicalizeLoadedWorkingSet(")
canonical_end = session.index("bool ModelAssetEditorSession::verifyLoadedWorkingSetCanonical(", canonical_start)
canonical_body = session[canonical_start:canonical_end]
if "sourceAnalysis.structuralInvalid" in canonical_body:
    raise AssertionError("explicit preparation still pre-rejects RAW topology before CanonicalMeshBuilder")
for token in ("const auto built = canonicalizeMesh(geometry.mesh)", "MESH PREPARATION INCOMPLETE"):
    if token not in canonical_body:
        raise AssertionError(f"explicit canonical preparation missing {token!r}")
if "auditPreflightGeometry(geometry.mesh)" in canonical_body:
    raise AssertionError("PREPARE MESHES still performs implicit topology audit/classification")
if "setGeometryTopologyClass" in canonical_body:
    raise AssertionError("PREPARE MESHES still mixes normalization with classification")
if 'resetMeshRepairDiagnostic(repairLogPath, m_asset)' not in canonical_body:
    raise AssertionError("PREPARE MESHES no longer resets its asset-local per-run repair log")
if 'build/logs' in canonical_body or 'model_asset_mesh_repair.log' in canonical_body:
    raise AssertionError("PREPARE MESHES regressed to CWD-relative/global diagnostics")
if "SOURCE BLOCKED: raw mesh was not exposed" in canonical_body:
    raise AssertionError("canonical preparation still masquerades as a SOURCE load blocker")
if "changedLods.insert(li)" not in canonical_body or "changedLodsOut->assign" not in canonical_body:
    raise AssertionError("PREPARE no longer reports exactly which LOD payloads changed")

prepare_command_start = session.index('if (command == "prepare_model_meshes")')
prepare_command_end = session.index('if (command == "analyze_model_preflight")', prepare_command_start)
prepare_command_body = session[prepare_command_start:prepare_command_end]
for token in (
    "std::vector<std::size_t> changedLods",
    'sendAsset(changedLods)',
    'sendAssetMetadata()',
    '[ModelAssetEditor][prepare]',
):
    if token not in prepare_command_body:
        raise AssertionError(f"PREPARE transport boundary missing {token!r}")
if "sendAsset();" in prepare_command_body:
    raise AssertionError("PREPARE still republishes every resident LOD unconditionally")

verify_start = session.index("bool ModelAssetEditorSession::verifyLoadedWorkingSetCanonical(")
verify_end = session.index("bool ModelAssetEditorSession::setGeometryTopologyClass(", verify_start)
verify_body = session[verify_start:verify_end]
if "structuralInvalid) continue" in verify_body:
    raise AssertionError("canonical verification lets invalid payloads bypass downstream records")

send_start = session.index("void ModelAssetEditorSession::sendAsset(")
send_end = session.index("void ModelAssetEditorSession::sendAssetMetadata", send_start)
send_body = session[send_start:send_end]
if "verifyLoadedWorkingSetCanonical" in send_body or "ASSET PAYLOAD BLOCKED" in send_body:
    raise AssertionError("sendAsset regressed into a load-time canonical gate")
for token in ("asset_binary_begin", "serializeAssetMetadata", "broadcastBinary", "encodeLodGeometryPayload", "reuseExistingPayloads"):
    if token not in send_body:
        raise AssertionError(f"sendAsset binary transport missing {token!r}")
if "serializeAsset(true)" in send_body or '"positions"' in send_body:
    raise AssertionError("sendAsset regressed to JSON geometry publication")
if "m_rawMeshSnapshots" in send_body:
    raise AssertionError("ordinary asset publication still retransmits session-only RAW snapshots")

select_start = session.index("bool ModelAssetEditorSession::selectAsset(")
select_end = session.index("bool ModelAssetEditorSession::saveAsset(", select_start)
select_body = session[select_start:select_end]
if "canonicalizeLoadedWorkingSet(" in select_body:
    raise AssertionError("selectAsset must load/restore/reimport without hidden canonicalization")
if "refreshSourceVariants(true, false)" not in select_body or "sendAsset();" not in select_body:
    raise AssertionError("selectAsset no longer preserves explicit source-import publication")
for token in (
    "latestSavedWizardCheckpoint",
    "resumeSavedPoint",
    "ModelAssetBinary::load(resumeCheckpoint.string(), loaded, &error)",
    '"wizard_checkpoint_restored"',
):
    if token not in select_body:
        raise AssertionError(f"last-saved checkpoint resume contract missing {token!r}")
if 'ModelAssetBinary::load(readPath.string(), loaded, &error)' not in select_body:
    raise AssertionError("production fallback disappeared when no editor checkpoint exists")

restore_start = session.index("bool ModelAssetEditorSession::restoreWizardCheckpoint(")
restore_end = session.index("bool ModelAssetEditorSession::scanRenderDuplicates(", restore_start)
restore_body = session[restore_start:restore_end]
if "canonicalizeLoadedWorkingSet(" in restore_body:
    raise AssertionError("checkpoint restore must not canonicalize implicitly")
if "sendAsset();" not in restore_body:
    raise AssertionError("checkpoint restore must publish exactly the restored payload")

for token in (
    'loadCheckpointEditorState(',
    'applyEditorAuthoringState(std::move(restoredEditorState))',
    'restoreWizardValidityAt(stage)',
):
    if token not in restore_body:
        raise AssertionError(f"checkpoint restore is not a self-contained workspace snapshot: {token!r}")

complete_start = session.index("bool ModelAssetEditorSession::completeWizardStage(")
complete_end = session.index("bool ModelAssetEditorSession::restoreWizardCheckpoint(", complete_start)
complete_body = session[complete_start:complete_end]
for token in (
    'checkpointValidity[stage] = validationPassed ? "complete" : "needs_fix"',
    'checkpointValidity[wizardStageOrder()[i]] = "not_started"',
    'const std::uint64_t checkpointSequence = m_nextCheckpointSequence',
    'writeCheckpointEditorState(stage, checkpointValidity, checkpointSequence',
    'm_nextCheckpointSequence = checkpointSequence + 1',
    'pruneWizardCheckpointsAfter(stage, &error)',
):
    if token not in complete_body:
        raise AssertionError(f"linear checkpoint SAVE contract missing {token!r}")

# 0.10.25: checkpoint persistence is independent of stage validation. The full
# package is written before validateWizardStage(), failed validation becomes
# NEEDS FIX, and only PASS may publish automatic progression.
save_pos = complete_body.index("ModelAssetBinary::save(checkpoint.string(), m_asset, &error)")
validate_pos = complete_body.index("validateWizardStage(stage, &validationError)")
if save_pos >= validate_pos:
    raise AssertionError("stage validation still blocks checkpoint package persistence")
for token in (
    'value.status = validationPassed ? "complete" : "needs_fix"',
    '"type", "wizard_state_patch"',
    '"type", "wizard_checkpoint_saved"',
    'if (validationPassed)',
):
    if token not in complete_body:
        raise AssertionError(f"validation-independent checkpoint contract missing {token!r}")
if 'sendAssetMetadata()' in complete_body or 'sendAsset();' in complete_body:
    raise AssertionError("checkpoint SAVE still republishes/rebuilds whole asset geometry")

load_data_start = session.index("bool ModelAssetEditorSession::loadLodData(")
load_lod_start = session.index("bool ModelAssetEditorSession::loadLodOnly(", load_data_start)
load_data_body = session[load_data_start:load_lod_start]
if "sendAsset(" in load_data_body or "sendLodPayload(" in load_data_body:
    raise AssertionError("backend-only LOD residency helper still publishes geometry as a side effect")
if "ModelAssetBinary::loadLod" not in load_data_body:
    raise AssertionError("backend-only LOD residency helper no longer owns the disk read boundary")

load_lod_end = session.index("bool ModelAssetEditorSession::ensureLodLoaded(", load_lod_start)
load_lod_body = session[load_lod_start:load_lod_end]
if "canonicalizeLoadedWorkingSet(" in load_lod_body:
    raise AssertionError("manual LOD load/reload must not canonicalize implicitly")
if "sendAsset({lodIndex})" not in load_lod_body or "sendLodPayload(lodIndex)" in load_lod_body:
    raise AssertionError("manual LOD load no longer preserves the old full-asset application terminal through a targeted transport delta")

send_lod_start = session.index("void ModelAssetEditorSession::sendLodPayload(")
send_lod_end = session.index("std::vector<std::string> ModelAssetEditorSession::sourceVariantReplacementIds", send_lod_start)
send_lod_body = session[send_lod_start:send_lod_end]
for token in ("includeRawSnapshots", "m_rawMeshSnapshots.find(lodIndex)", 'raw="'):
    if token not in send_lod_body:
        raise AssertionError(f"explicit RAW viewport payload path missing {token!r}")

preflight_start = session.index("bool ModelAssetEditorSession::analyzeModelPreflight()")
preflight_end = session.index("bool ModelAssetEditorSession::canonicalizeLoadedWorkingSet(", preflight_start)
preflight_body = session[preflight_start:preflight_end]
if "canonicalizeLoadedWorkingSet(" in preflight_body or "canonicalizeMesh(" in preflight_body:
    raise AssertionError("ANALYZE regressed into a mutation stage")
if "verifyLoadedWorkingSetCanonical" in preflight_body:
    raise AssertionError("ANALYZE must accept RAW/mixed geometry instead of refusing to inspect it")
for token in ("needsPreparation", 'action = "prepare_required"'):
    if token not in preflight_body:
        raise AssertionError(f"mixed RAW/canonical Preflight reporting missing {token!r}")

lod_analysis_start = session.index("bool ModelAssetEditorSession::analyzeLodRequirements(")
lod_analysis_end = session.index("bool ModelAssetEditorSession::previewLodComponentCull(", lod_analysis_start)
lod_analysis_body = session[lod_analysis_start:lod_analysis_end]
if "canonicalizeLoadedWorkingSet(" in lod_analysis_body or "canonicalizeMesh(" in lod_analysis_body:
    raise AssertionError("LOD analysis regressed into a canonicalization/repair stage")
if "verifyLoadedWorkingSetCanonical" not in lod_analysis_body:
    raise AssertionError("LOD analysis must still gate on explicitly prepared canonical geometry")

refresh_start = session.index("bool ModelAssetEditorSession::refreshSourceVariants(")
refresh_end = session.index("void ModelAssetEditorSession::handleMessage(", refresh_start)
refresh_body = session[refresh_start:refresh_end]
if "canonicalizeMesh(mesh)" in refresh_body or "canonicalizeLoadedWorkingSet(" in refresh_body:
    raise AssertionError("source variant refresh must remain a literal RAW reload")
for token in ("sameMeshLodExact(existing->mesh, mesh)", "m_meshPreparationRecords", "canonicalEvidenceChanged"):
    if token not in refresh_body:
        raise AssertionError(f"RAW source refresh invalidation contract missing {token!r}")
if "sendAsset(std::vector<std::size_t>(changedLods.begin(), changedLods.end()))" not in refresh_body:
    raise AssertionError("source refresh retransmits unchanged resident LOD geometry instead of using an asset-terminal transport delta")

require("src/assets/compiled/models/.gitignore", "Compiled model packages")

cmake = text("CMakeLists.txt")
for token in ("ELITE_BUILD_ASSET_EDITOR", "EliteModelAsset", "NativeObjImporter.cpp", "CanonicalMeshBuilder.cpp", "GeometryInstanceFitter.cpp", "ModelAssetEditorWire.cpp", "ModelAssetMigration.cpp"):
    if token not in cmake:
        raise AssertionError(f"asset editor v4 target missing {token!r}")

# Editor-first gate: game runtime renderer still does not load the new package.
for game_path in (
    "src/scene/SceneRenderer.cpp",
    "src/render/geometry/AssemblyGpuLibrary.cpp",
    "src/game/geometry/AssemblyMeshLibrary.cpp",
):
    if "ModelAssetBinary" in text(game_path):
        raise AssertionError(f"{game_path} migrated runtime model loading before editor gate")



# Additional replacement meshes are discovered recursively below each real
# LOD<N> directory. Filenames/folder names are reload/organization pointers only;
# persistent authoring ids and replacement compatibility live in wizard_state.json.
require(
    "src/model_asset/ModelAssetVariantNaming.h",
    "SourceRenderVariantPrefix",
    "LegacyRenderVariantMarker",
    "makeRenderVariantGeometryId",
    "renderVariantIdentity",
)
require(
    "src/assets/models/VARIANTS.md",
    "recursively scans the whole",
    "opaque persistent",
    "base visual id",
    "No LOD association is inferred from matching filenames",
    "wizard_state.json",
)
require(
    "tools/model_asset_editor/RuntimeAssemblyImporter.cpp",
    "discoverAdditionalLodMeshes",
    "runtimeAssemblyLodSourcePaths",
    "recursive_directory_iterator",
)
require(
    "tools/model_asset_editor/ModelAssetEditorSession.cpp",
    "reconcileAuthoringVisualRegistry",
    "allocateBaseVisualId",
    "allocateSourceVariantId",
    '"baseVisuals"',
    '"sourceExtraMeshes"',
    '"replacesBaseVisualIds"',
    'command == "set_source_variant_replacement"',
    "sameMeshLodExact",
    "unchanged",
)
require(
    "src/assets/webui/model_asset_editor.html",
    "variantPreviewByNode",
    "geometryVariantSelected",
    "renderVariantAssignment",
    "baseVisualId",
    "set_source_variant_replacement",
    "wizardExtraMeshTable",
    "wizardBaseReplacementTable",
    "previewToggle",
    "wizardUnusedGeometrySummary",
    "checkpointCurrent",
    "updateActionAvailability",
    "initWorldAxes",
    "['+X'",
    "['-X'",
    "['+Y'",
    "['-Y'",
    "['+Z'",
    "['-Z'",
)
# stable extra-mesh authoring ids: source filename stems must not define ids or
# cross-LOD pairing in the importer/session refresh path.
importer_cpp = text("tools/model_asset_editor/RuntimeAssemblyImporter.cpp")
if "entry.path().stem().string()" in importer_cpp:
    raise AssertionError("additional mesh authoring identity still depends on OBJ filename stem")

# Metadata-only synchronization must remain an actual transport boundary, not merely
# a UI label. Full mesh payloads are allowed on full asset snapshots only.
session_cpp = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
if "const bool protectedVariant = isRenderVariantGeometryId" not in session_cpp:
    raise AssertionError("source variants can be deleted by unused-geometry cleanup")
for token in ("refreshSourceVariants", 'command == "refresh_source_variants"', "discoverAdditionalLodMeshes", "runtimeAssemblyLodSourcePaths"):
    if token not in session_cpp:
        raise AssertionError(f"source-variant refresh workflow missing {token!r}")
for token in ("loadAllDeclaredLodsForSource", "refreshSourceVariants(true, false)", 'sourceOwned ? "source" : "geometry"'):
    if token not in session_cpp:
        raise AssertionError(f"SOURCE complete-authoring-set contract missing {token!r}")
source_ui = text("src/assets/webui/model_asset_editor.html")
for token in ("wizardSourceRefreshBtn", "sourceInventoryRow", "model_editor.source.loaded"):
    if token not in source_ui:
        raise AssertionError(f"SOURCE inventory UI contract missing {token!r}")
for token in (
    '"type", "asset_metadata"',
    'serializeAssetMetadata()',
    '"type", "asset_binary_begin"',
    '"type", "lod_payload_binary_begin"',
    'wizardCheckpointEditorStatePath',
    'productionEditorStatePath',
    '"editor_state.json"',
    '"production_state.json"',
    'writeCheckpointEditorState',
    'loadCheckpointEditorState',
    'writeProductionEditorState',
    'loadProductionEditorState',
    'captureStageValidity',
    'applyStageValidity',
    'latestSavedWizardCheckpoint',
    'checkpointSequenceForStage',
    'm_nextCheckpointSequence',
    'pruneWizardCheckpointsAfter',
):
    if token not in session_cpp:
        raise AssertionError(f"checkpoint/workspace lifecycle architecture missing {token!r}")

# Checkpoint deletion is legal only at explicit COMPLETE STAGE + CHECKPOINT.
invalidate_start = session_cpp.index("void ModelAssetEditorSession::invalidateWizardFrom")
invalidate_end = session_cpp.index("nlohmann::json ModelAssetEditorSession::serializeWizard", invalidate_start)
if "pruneWizardCheckpointsAfter" in session_cpp[invalidate_start:invalidate_end] or "remove_all" in session_cpp[invalidate_start:invalidate_end]:
    raise AssertionError("ordinary invalidation/reimport can still delete saved checkpoints")
if "pruneWizardCheckpointsAfter" in restore_body or "remove_all" in restore_body:
    raise AssertionError("RESTORE must not delete later saved checkpoints")
if "std::filesystem::remove_all(directory, ec)" not in session_cpp:
    raise AssertionError("explicit checkpoint SAVE no longer prunes later-stage snapshots")

latest_start = session_cpp.index("std::filesystem::path ModelAssetEditorSession::latestSavedWizardCheckpoint")
latest_end = session_cpp.index("bool ModelAssetEditorSession::pruneWizardCheckpointsAfter", latest_start)
latest_body = session_cpp[latest_start:latest_end]
for token in (
    'checkpointSequenceForStage(currentStage)',
    'bestSequence',
    'wizardCheckpointEditorStatePath(currentStage)',
    'last_write_time(timeSource, ec)',
):
    if token not in latest_body:
        raise AssertionError(f"checkpoint sequence resume authority missing {token!r}")
if 'last_write_time(checkpoint, ec)' in latest_body:
    raise AssertionError("new checkpoint resume still uses package mtime instead of logical checkpoint sequence")

if session_cpp.count('m_nextCheckpointSequence = checkpointSequence + 1') != 1:
    raise AssertionError("checkpoint timeline must advance in exactly one explicit SAVE path")
if '++m_nextCheckpointSequence' in session_cpp or 'm_nextCheckpointSequence++' in session_cpp:
    raise AssertionError("checkpoint sequence can advance outside the explicit SAVE assignment")

load_state_start = session_cpp.index("void ModelAssetEditorSession::loadWizardState()")
load_state_end = session_cpp.index("bool ModelAssetEditorSession::writeWizardState() const", load_state_start)
load_state_body = session_cpp[load_state_start:load_state_end]
for token in (
    'wizard_state.json is not a persisted working copy',
    'const auto checkpoint = wizardCheckpointPath(id)',
    'm_nextCheckpointSequence = 1',
    'checkpointSequenceForStage(id)',
):
    if token not in load_state_body:
        raise AssertionError(f"session/checkpoint index separation missing {token!r}")
for forbidden in ('parseEditorAuthoringState(', 'applyEditorAuthoringState(std::move(authoring))'):
    if forbidden in load_state_body:
        raise AssertionError(f"mutable wizard_state can still become the saved resume head: {forbidden!r}")

write_state_start = session_cpp.index("bool ModelAssetEditorSession::writeWizardState() const")
write_state_end = session_cpp.index("std::string ModelAssetEditorSession::allocateBaseVisualId", write_state_start)
write_state_body = session_cpp[write_state_start:write_state_end]
if 'serializeEditorAuthoringState(captureEditorAuthoringState())' in write_state_body:
    raise AssertionError("wizard_state still persists an unbound authoring working copy")
if 'model_asset_editor_session_index' not in write_state_body:
    raise AssertionError("wizard_state is no longer explicitly a session/checkpoint index")

select_start2 = session_cpp.index("bool ModelAssetEditorSession::selectAsset(")
select_end2 = session_cpp.index("bool ModelAssetEditorSession::saveAsset()", select_start2)
select_lifecycle = session_cpp[select_start2:select_end2]
for token in (
    'latestSavedWizardCheckpoint(&resumedCheckpointStage)',
    'the last explicitly saved stage checkpoint is',
    'Source assembly reloaded into RAM. Saved checkpoints are unchanged',
    'Unsaved reimport/reload/edit state from the previous session was discarded',
):
    if token not in select_lifecycle:
        raise AssertionError(f"linear editor resume contract missing {token!r}")

save_start2 = session_cpp.index("bool ModelAssetEditorSession::saveAsset()")
save_end2 = session_cpp.index("nlohmann::json ModelAssetEditorSession::serializeAssetMetadata", save_start2)
save_lifecycle = session_cpp[save_start2:save_end2]
if 'writeProductionEditorState(&error)' not in save_lifecycle:
    raise AssertionError("SAVE ALL no longer binds editor/stage evidence to production output")

checkpoint_state_start = session_cpp.index("bool ModelAssetEditorSession::writeCheckpointEditorState")
checkpoint_state_end = session_cpp.index("void ModelAssetEditorSession::loadWizardState()", checkpoint_state_start)
checkpoint_state_body = session_cpp[checkpoint_state_start:checkpoint_state_end]
for token in (
    'state["stages"] = serializeStageValidity(validity)',
    'schemaVersion"] = 10',
    'state["checkpointSequence"] = checkpointSequence',
):
    if token not in checkpoint_state_body:
        raise AssertionError(f"checkpoint is not a complete stage-validity snapshot: {token!r}")

# Commands already present for reserved future stages must participate in the
# same stage-validity contract now, before those wizard pages are enabled.
planned_mutation_stages = {
    "convert_source_basis": "source",
    "set_node_transform": "semantics",
    "set_render_node_semantic": "semantics",
    "set_joint": "semantics",
    "add_socket": "semantics",
    "set_physics": "physics",
    "estimate_physics": "physics",
    "add_collision": "physics",
    "set_collision": "physics",
    "set_node_default_state": "damage",
    "add_state_variant": "damage",
    "set_render_node_states": "damage",
    "add_hit_region": "damage",
    "add_opening": "damage",
    "add_repair_target": "damage",
}
for command_name, stage_name in planned_mutation_stages.items():
    marker = f'if (command == "{command_name}")'
    start = session_cpp.index(marker)
    next_command = session_cpp.find('if (command == "', start + len(marker))
    body = session_cpp[start: next_command if next_command != -1 else len(session_cpp)]
    expected = f'invalidateWizardFrom("{stage_name}")'
    if expected not in body:
        raise AssertionError(
            f"planned {stage_name.upper()} mutation {command_name!r} does not invalidate its stage/downstream checkpoints"
        )

for token in (
    'restoreOnly',
    'Other checkpoints remain stored',
    'Other checkpoints are preserved',
    'restoreOnly||!groups.includes(state.wizardStage)',
    'Every later-stage checkpoint is removed',
):
    if token not in web:
        raise AssertionError(f"linear checkpoint UI contract missing {token!r}")

for forbidden in ('g["positions"]', 'g["normals"]', 'g["indices"]', 'rawJson["positions"]', 'serializeAsset(true)'):
    if forbidden in session_cpp:
        raise AssertionError(f"bulk geometry returned to JSON transport: {forbidden!r}")

require(
    "tools/model_asset_editor/ModelAssetEditorWire.cpp",
    "'E','L','W','I','R','0','0','1'",
    "WireVersion",
    "encodeLodGeometryPayload",
    "writeMeshArrays",
    "writeRawMeshArrays",
)
require(
    "src/ui/html/HtmlUiServer.cpp",
    "broadcastBinary",
    "websocketpp::frame::opcode::binary",
)
require(
    "src/model_asset/ModelAssetBinary.cpp",
    "memory cursor",
    "std::ios::binary | std::ios::ate",
    "Reader(const std::uint8_t* data, std::size_t size)",
)
for token in (
    "asset_binary_begin",
    "lod_payload_binary_begin",
    "ELWIR001",
    "decodeEditorLodGeometry",
    "applyEditorLodGeometry",
    "reuseEditorLodGeometry",
    "reuseExistingPayloads",
    "handle({type:'asset'",
    "handle({type:'lod_payload'",
    "binaryType='arraybuffer'",
):
    if token not in web:
        raise AssertionError(f"binary geometry transport missing browser terminal {token!r}")

# Binary arrays must be adapted back into the exact Three.js types expected by
# the preserved viewport terminal. BufferGeometry.setIndex() treats a typed
# array as an already-built BufferAttribute, so passing Uint32Array directly
# silently produces a non-renderable indexed mesh.
for token in (
    "ArrayBuffer.isView(indices)?new THREE.BufferAttribute(indices,1):indices",
    "edges=new Array(edgeCount)",
    "logEditorWireTiming",
    "activeLodNeedsRawSource",
    "includeRaw:true",
):
    if token not in web:
        raise AssertionError(f"binary viewport compatibility/perf guard missing {token!r}")

wire_cpp = text("tools/model_asset_editor/ModelAssetEditorWire.cpp")
for token in (
    "Writer w(estimatedPayloadBytes(lod, rawSnapshots))",
    "data[offset++] = value",
    "return w.finish()",
):
    if token not in wire_cpp:
        raise AssertionError(f"binary writer regressed to byte-at-a-time vector growth: {token!r}")

invalidate_start = session_cpp.index("void ModelAssetEditorSession::invalidateWizardFrom")
invalidate_end = session_cpp.index("nlohmann::json ModelAssetEditorSession::serializeWizard", invalidate_start)
invalidate_body = session_cpp[invalidate_start:invalidate_end]
for token in (
    'for (std::size_t i = first; i < order.size(); ++i)',
    'value.status = "stale"',
    'value.status = "not_started"',
):
    if token not in invalidate_body:
        raise AssertionError(f"future-stage checkpoint invalidation contract missing {token!r}")

web_sync = text("src/assets/webui/model_asset_editor.html")
for token in (
    "mergeAssetMetadata",
    "retainGeometryPayload",
    "cloneGeometryPayload",
    "pruneGeometryCache",
    "rebuildScene(preserveGeometryCache=false)",
):
    if token not in web_sync:
        raise AssertionError(f"browser geometry-cache contract missing {token!r}")

metadata_only_commands = (
    "set_node_transform",
    "set_render_node_transform",
    "set_render_node_geometry",
    "fit_render_node_as_instance",
    "consolidate_render_duplicates",
    "break_render_node_instance",
    "duplicate_render_node_instance",
    "create_radial_render_instances",
    "delete_render_node",
    "delete_unused_geometries",
    "set_edge_render_mask",
    "add_collision",
    "set_collision",
    "add_socket",
    "set_socket",
)
for command in metadata_only_commands:
    start = session_cpp.find(f'if (command == "{command}")')
    if start < 0:
        raise AssertionError(f"metadata-only command disappeared: {command}")
    next_if = session_cpp.find('\n        if (command == "', start + 1)
    block = session_cpp[start: next_if if next_if >= 0 else len(session_cpp)]
    if "sendAsset();" in block:
        raise AssertionError(f"command {command!r} regressed to full mesh retransmission")
    if "sendAssetMetadata" not in block:
        raise AssertionError(f"command {command!r} does not publish metadata update")

# SURFACES has a narrower metadata transport than generic metadata-only commands.
surface_mode_start = session_cpp.index('if (command == "set_surface_mode")')
surface_mode_end = session_cpp.index('if (command == "set_material_definition")', surface_mode_start)
surface_mode_block = session_cpp[surface_mode_start:surface_mode_end]
if "sendSurfaceMetadataPatch" not in surface_mode_block:
    raise AssertionError("set_surface_mode no longer publishes its targeted metadata patch")
for forbidden in ("sendAssetMetadata", "sendAsset();", "analyzeModelPreflight"):
    if forbidden in surface_mode_block:
        raise AssertionError(f"set_surface_mode retained heavy side effect {forbidden!r}")

# Canonicalization is an explicit authoring operation. Loading may expose RAW
# geometry; PREPARE mutates it, ANALYZE is read-only, and downstream LOD work
# still requires an exact-payload canonical record.
for token in (
    "analyzeModelPreflight",
    "canonicalizeLoadedWorkingSet",
    "verifyLoadedWorkingSetCanonical",
    "setGeometryTopologyClass",
    "modelPreflightReadyForLod",
    "modelPreflightAllLoadedReady",
    "canonicalizeMesh",
    "canonicalMeshFingerprint",
    "CanonicalMeshAlgorithmId",
    "m_meshPreparationRecords",
    "meshPreparationRecords",
    "geometryTopologyClasses",
    'state["schemaVersion"] = 8',
):
    if token not in session_cpp:
        raise AssertionError(f"canonical SOURCE/preflight backend contract missing {token!r}")
canonical_cpp = text("tools/model_asset_editor/CanonicalMeshBuilder.cpp")
for token in (
    "analyzeCanonicalMesh",
    "buildTopologicalPointMap",
    "solveOrientation",
    "repairTopologyAndOrientationWithLibigl",
    "igl::split_nonmanifold",
    "igl::embree::reorient_facets_raycast",
    "EdgeCanonicalTopology",
    "sourceNonManifoldEdgeCount",
    "rebuildRenderVertices",
):
    if token not in canonical_cpp:
        raise AssertionError(f"canonical authoring / explicit analysis contract missing {token!r}")
canonicalize_start = canonical_cpp.index("CanonicalMeshBuildResult canonicalizeMesh(MeshLod& mesh)")
canonicalize_body = canonical_cpp[canonicalize_start:canonical_cpp.index("std::uint64_t canonicalMeshFingerprint", canonicalize_start)]
for required in ("repairTopologyAndOrientationWithLibigl(", "rebuildRenderVertices("):
    if required not in canonicalize_body:
        raise AssertionError(f"PREPARE MESHES lost canonical orientation stage {required!r}")
for forbidden in ("normalizeRuntimeMeshTopology(", "analyzeCanonicalMesh(candidate)", "MaxStabilizationPasses", "const MeshLod original = mesh", "orientOpenComponentsByEnvelope(", "radialScore", "OpenOrientationMinConfidence"):
    if forbidden in canonicalize_body:
        raise AssertionError(f"PREPARE MESHES regressed to runtime-only, heuristic, or multi-pass behavior {forbidden!r}")
for forbidden in ("orientOpenComponentsByEnvelope", "radialScore", "OpenOrientationMinConfidence"):
    if forbidden in canonical_cpp:
        raise AssertionError(f"removed radial/open-component orientation heuristic returned: {forbidden!r}")
model_asset_tests = text("tests/model_asset/ModelAssetBinaryTests.cpp")
for token in (
    "testCanonicalBuilderRepairsWindingAndOutwardNormals",
    "testCanonicalBuilderClosedPlateBreachContracts",
    "testCanonicalBuilderOrientsBreachedShellWithEmbree",
    "testCanonicalBuilderRemovesGarbageAndPreservesUvSeams",
    "testCanonicalBuilderCollapsesAuthoredNormalOnlySplits",
    "testCanonicalPreparationKeepsCoincidentSheetsIndependent",
    "testCanonicalPreparationRebuildsHardNormalIslands",
    "testRuntimeNormalizerRemainsTolerantRenderContract",
    "testCanonicalBuilderFingerprintTracksStructuralPayload",
    "testPreparationRejectsUnreadableAndRepairsNonManifold",
    "split_nonmanifold did not split a genuine three-face geometric edge",
    "closed shell remained inward after canonical preparation",
):
    if token not in model_asset_tests:
        raise AssertionError(f"canonical-authoring behavioral regression missing {token!r}")
for token in (
    "modelPreflightPrepareBtn",
    "modelPreflightCheckBtn",
    "renderModelPreflightPanel",
    "model_preflight_result",
    "set_geometry_topology_class",
    "model_editor.preflight.class_thin",
    "model_editor.preflight.class_breached",
    "model_editor.preflight.canonical_note",
    "model_editor.preflight.workflow",
    "meshViewportMode",
    "ИСХОДНИК",
    "БЕЗ ОТСЕЧЕНИЯ",
    "РАБОЧИЙ",
):
    if token not in web_sync:
        raise AssertionError(f"classification-only Preflight Web UI contract missing {token!r}")
for forbidden in (
    "modelPreflightFixBtn",
    "modelPreflightRuntimeBtn",
    "apply_mesh_preparation",
    "safe_fix_model_preflight",
    "runtimeNormalizedThreeGeometry",
):
    if forbidden in web_sync:
        raise AssertionError(f"obsolete manual/raw Preflight UI returned {forbidden!r}")

# LOD generator v1 uses preview for inspection and an explicit APPLY boundary for
# authored LOD documents. It must use the fixed project authoring ceiling, analyze
# component thickness rather than filename/triangle count heuristics, and include
# both main and additional/replacement meshes in every generated LOD.
require(
    "src/render/RenderResolutionPolicy.h",
    "MaximumSupportedRenderWidth = 2560",
    "MaximumSupportedRenderHeight = 1440",
)
require(
    "src/window/Window.cpp",
    "elite::render::MaximumSupportedRenderWidth",
    "elite::render::MaximumSupportedRenderHeight",
)
for token in (
    "LodReferenceWidthPx = render::MaximumSupportedRenderWidth",
    "LodReferenceHeightPx = render::MaximumSupportedRenderHeight",
    "LodReferenceVerticalFovDeg = 70.0",
    "LodVisibilityCutoffPx = 2.0",
    "analyzeConnectedComponents",
    "component.featureMeters = component.principalExtents.y",
    "compressTriangleRanges",
    "analyzeLodRequirements",
    "previewLodComponentCull",
    "previewLodCoplanarCollapse",
    "analyzeCoplanarCollapse",
    "buildGeneratedComponentCullLod",
    "GeneratedLodComponentCullAlgorithmId",
    "applyGeneratedLods",
    "apply_generated_lods",
):
    if token not in session_cpp:
        raise AssertionError(f"LOD generator preview contract missing {token!r}")
for token in (
    "lodGeneratorAnalyzeBtn",
    "renderLodGeneratorPanel",
    "lodGeneratorPreviewGeometry",
    "model_editor.lod_generator.show_lod0",
    "state.lodGeneratorLevel=0",
    "lod_analysis_result",
    "lod_generator_preview_result",
    "removedTriangleRanges",
    "addedTriangles",
    "preview_lod_coplanar_collapse",
    "lodDiagnosticActive",
    "lodDiagnosticFaceNormalsBtn",
    "lodGeneratorApplyBtn",
    "lodGeneratorApplyLevels",
    "lodGeneratorMeshSelection",
    "apply_generated_lods",
    "model_editor.lod_generator.apply",
):
    if token not in web_sync:
        raise AssertionError(f"LOD generator Web UI contract missing {token!r}")


# 0.10.18: generated LOD authoring is full-asset and transactional. Additional
# replacement meshes are generated even though they have no RenderNode usage.
analysis_start = session_cpp.index("bool ModelAssetEditorSession::analyzeLodRequirements(")
analysis_end = session_cpp.index("bool ModelAssetEditorSession::previewLodComponentCull(", analysis_start)
analysis_body = session_cpp[analysis_start:analysis_end]
if "if (isRenderVariantGeometryId(geometry.id)) continue" in analysis_body or "if (usage[geometryIndex] == 0) continue" in analysis_body:
    raise AssertionError("LOD analysis still skips additional/unbound geometry")
preview_start = session_cpp.index("bool ModelAssetEditorSession::previewLodComponentCull(")
preview_end = session_cpp.index("bool ModelAssetEditorSession::applyGeneratedLods(", preview_start)
preview_body = session_cpp[preview_start:preview_end]
if "if (isRenderVariantGeometryId(geometry.id)) continue" in preview_body or "if (usage[geometryIndex] == 0) continue" in preview_body:
    raise AssertionError("LOD preview still skips additional/unbound geometry")
apply_start = session_cpp.index("bool ModelAssetEditorSession::applyGeneratedLods(")
apply_end = session_cpp.index("bool ModelAssetEditorSession::previewLodCoplanarCollapse(", apply_start)
apply_body = session_cpp[apply_start:apply_end]
for token in (
    'generated.sourceKind = "generated"',
    "generated.generatedFromLod",
    "candidates.reserve(selected.size())",
    "analyzeCanonicalMesh(candidate.lod.geometries[gi].mesh)",
    "m_baseVisualIds[selection.level] = sourceBaseVisuals->second",
    "m_sourceExtraMeshIds[selection.level] = sourceExtraIds->second",
    "GeneratedLodComponentCullAlgorithmId",
    'invalidateWizardFrom("lods")',
):
    if token not in session_cpp:
        raise AssertionError(f"generated LOD authoring contract missing {token!r}")
for token in (
    "lodGeneratorApplyBtn",
    "data-lod-apply",
    "lodGeneratorMeshSelect",
    "ADDITIONAL / REPLACEMENT MESHES",
    "APPLY SELECTED LODS",
):
    if token not in web_sync:
        raise AssertionError(f"generated LOD authoring UI missing {token!r}")


# 0.10.19: applying a large full-asset LOD set must update authored metadata
# immediately instead of serializing every generated vertex/index array into one
# browser message. Individual LOD payloads are fetched on demand. Main-mesh
# isolation must keep the transform hierarchy alive and filter mesh visibility
# only.
for token in (
    "sendLodPayload",
    "request_lod_payload",
    "invalidatedLodPayloads",
    '"type", "lod_payload_binary_begin"',
    "encodeLodGeometryPayload",
    "sendAssetMetadata({{\"invalidatedLodPayloads\", invalidatedPayloads}})",
):
    if token not in session_cpp:
        raise AssertionError(f"LOD apply payload boundary missing {token!r}")
for token in (
    "lodGeneratorApplying",
    "lodGeneratorAppliedLevels",
    "lodGeneratorPendingApplyLevels",
    "lodGeneratorActions",
    "GENERATED LODS APPLIED",
    "AUTHORED LOD UPDATED",
    "request_lod_payload",
    "invalidatedLodPayloads",
    "mesh.visible=lodGeneratorNodePassesMeshFilter(n)",
    "group.visible=stateApplies(n)",
):
    if token not in web_sync:
        raise AssertionError(f"LOD apply feedback/isolation UI missing {token!r}")
if "group.visible=stateApplies(n)&&lodGeneratorNodePassesMeshFilter(n)" in web_sync:
    raise AssertionError("LOD main-mesh filter still disables transform parent groups")

# 0.10.17: surface classification is not an LODS/LOD-analysis gate.
ready_start = session_cpp.index("bool ModelAssetEditorSession::modelPreflightReadyForLod(")
ready_end = session_cpp.index("bool ModelAssetEditorSession::modelPreflightAllLoadedReady(", ready_start)
ready_body = session_cpp[ready_start:ready_end]
for forbidden in ("needs an explicit target geometry class", "surface mode does not match geometry class", "m_geometryTopologyClasses"):
    if forbidden in ready_body:
        raise AssertionError(f"LOD technical readiness still depends on SURFACES authoring: {forbidden!r}")
validate_stage_start = session_cpp.index("bool ModelAssetEditorSession::validateWizardStage(")
validate_stage_end = session_cpp.index("bool ModelAssetEditorSession::completeWizardStage(", validate_stage_start)
validate_stage_body = session_cpp[validate_stage_start:validate_stage_end]
if "modelPreflightAllLoadedReady" in validate_stage_body:
    raise AssertionError("LODS stage validation still blocks on surface classification")
if "verifyLoadedWorkingSetCanonical" not in validate_stage_body:
    raise AssertionError("LODS stage validation lost canonical geometry validation")
if "analyze.disabled=!lods[0]?.loaded||!state.modelPreflight?.readyForLod" in web_sync or "analyze.disabled=!p.readyForLod" in web_sync:
    raise AssertionError("LOD0 Analyze button is still disabled by Preflight classification state")

# Editor developer artifacts must not depend on process CWD.
for forbidden in ('std::filesystem::path("build") / "logs"', 'model_asset_mesh_repair.log', 'model_asset_instance_fit.log'):
    if forbidden in session_cpp:
        raise AssertionError(f"editor diagnostics regressed to legacy global/CWD path {forbidden!r}")
for token in ('wizardWorkspacePath() / "logs" / fileName', 'wizardLogPath("mesh_repair.log")', 'wizardLogPath("instance_fit.log")'):
    if token not in session_cpp:
        raise AssertionError(f"stable asset-local editor log contract missing {token!r}")
cmake = text("CMakeLists.txt")
for token in ('ELITE_MODEL_ASSET_EDITOR_ARTIFACT_ROOT', 'build/tools/model_asset_editor', 'RUNTIME_OUTPUT_DIRECTORY "${ELITE_MODEL_ASSET_EDITOR_BIN_DIR}"', 'ELITE_EDITOR_RUNTIME_ROOT'):
    if token not in cmake:
        raise AssertionError(f"stable Model Asset Editor binary layout missing {token!r}")

editor_main = text("tools/model_asset_editor/main.cpp")
for token in ("ELITE_EDITOR_RUNTIME_ROOT", "std::filesystem::current_path", "model_asset_editor_ui.pak"):
    if token not in editor_main:
        raise AssertionError(f"editor runtime-root/UI-pack contract missing {token!r}")
for token in (
    'ELITE_EDITOR_RUNTIME_ROOT=\\"${ELITE_MODEL_ASSET_EDITOR_ARTIFACT_ROOT}\\"',
    'model_asset_editor_ui.pak',
    'build_model_asset_editor_ui_pack',
    'copy_model_asset_editor_webui',
):
    if token not in cmake:
        raise AssertionError(f"editor-owned runtime/UI package contract missing {token!r}")
if 'add_dependencies(EliteAssetEditor copy_assets)' in cmake:
    raise AssertionError("Model Asset Editor still depends on the shared/game asset deployment tree")
shared_asset_guard = 'if(TARGET EliteGame OR TARGET EliteServer)\n    elite_copy_asset_tree(\n        copy_static_assets'
if shared_asset_guard not in cmake:
    raise AssertionError("shared copy_assets rules are not isolated from editor-only build trees")
html_server_cpp = text("src/ui/html/HtmlUiServer.cpp")
if 'elite_ui.pak' in html_server_cpp:
    raise AssertionError("HtmlUiServer still auto-discovers the obsolete universal elite_ui.pak")
build_helper = text("build_asset_editor_mingw64.sh")
for token in ('build/tools/model_asset_editor/bin', 'model_asset_libigl_spike'):
    if token not in build_helper:
        raise AssertionError(f"editor build helper lost stable output contract {token!r}")

# Protected editor capabilities are a hard four-layer contract. A feature is not
# considered preserved merely because its C++ implementation still exists: the
# data model, backend command, visible UI entry point and regression test must all
# survive future editor rewrites.
capability_path = ROOT / "tools/model_asset_editor/EDITOR_CAPABILITIES.json"
capability_doc = json.loads(capability_path.read_text(encoding="utf-8"))
if capability_doc.get("schema_version") != 1:
    raise AssertionError("unsupported EDITOR_CAPABILITIES schema")
for capability in capability_doc.get("protected_capabilities", []):
    capability_id = capability.get("id", "<missing>")
    contracts = capability.get("contracts", {})
    for layer in ("data_model", "backend", "ui", "test"):
        contract = contracts.get(layer)
        if not contract:
            raise AssertionError(f"capability {capability_id!r} missing {layer} contract")
        contract_path = contract.get("path", "")
        tokens = contract.get("tokens", [])
        if not contract_path or not tokens:
            raise AssertionError(f"capability {capability_id!r} has incomplete {layer} contract")
        require(contract_path, *tokens)

for protected_id in (
    "geometry_instance_fit",
    "independent_render_lods",
    "semantic_damage_states",
    "source_reimport_read_only",
    "wizard_checkpoints",
    "incremental_editor_sync",
    "source_render_variants",
    "lod_generator_preview",
    "lod_generator_authoring",
    "model_preflight",
    "surface_authoring",
):
    if protected_id not in {c.get("id") for c in capability_doc.get("protected_capabilities", [])}:
        raise AssertionError(f"protected editor capability disappeared: {protected_id}")

# 0.10.20: GEOMETRY is a complete per-LOD workspace again, not only the
# rigid-fit comparison table. Entering the stage clears transient LOD/replacement
# preview state, the active LOD selector comes first, and instance arrays plus
# replacement compatibility remain visible first-class authoring tools.
for token in (
    "resetGeometryViewportState",
    "wizardGeometryLodSelect",
    "wizardGeometryMainMeshes",
    "wizardGeometryExtraMeshes",
    "wizardGeometryShowAllBtn",
    "geometryMeshSelection",
    "geometryNodePassesMeshFilter",
    "addGeometryStandaloneSelection",
    "wizardGeometryDuplicateBtn",
    "wizardGeometryBreakBtn",
    "wizardGeometryRadialBtn",
    "wizardBaseReplacementTable",
    "wizardVariantPreviewResetBtn",
):
    if token not in web_sync:
        raise AssertionError(f"restored GEOMETRY workspace missing {token!r}")
for token in (
    'state.variantPreviewByNode.clear()',
    "state.geometryMeshSelection='all'",
    "state.lodGeneratorMeshSelection='all'",
    "state.meshViewportMode='working'",
):
    if token not in web_sync:
        raise AssertionError(f"GEOMETRY stage-entry reset missing {token!r}")
if "mesh.visible=lodGeneratorNodePassesMeshFilter(n)&&geometryNodePassesMeshFilter(n)" not in web_sync:
    raise AssertionError("GEOMETRY single-mesh preview is not applied at mesh visibility boundary")
if "if(next)setWizardStage(next,true)" not in web_sync:
    raise AssertionError("automatic wizard progression bypasses the unified stage-entry transaction")

# 0.10.21: SURFACES is the first implemented stage after GEOMETRY. It owns
# surface intent, material properties, material assignment audit and its own
# checkpoint. It must not invalidate LODS/GEOMETRY merely because an author
# resolves a surface classification.
for token in (
    'const bool implemented = i < 4',
    'stageIndex >= 4',
    'else if (stage == "surfaces")',
    'SURFACES validation failed',
    'PreflightTopologyClass::ThinOneSided',
    'set_material_definition',
    'assign_unassigned_material',
    'invalidateWizardFrom("surfaces")',
    '"surfaceIntent"',
    '"materialSlots"',
    '"unassignedMaterialTriangles"',
):
    if token not in session_cpp:
        raise AssertionError(f"SURFACES backend contract missing {token!r}")
for token in (
    'wizardSurfaceLodSelect',
    'wizardSurfaceGeometryTable',
    'wizardSurfaceIntent',
    'wizardSurfaceMaterialSelect',
    'wizardSurfaceApplyMaterialBtn',
    'surfaceGeometrySelection',
    'surfaceMaterialSelection',
    'configureSurfacePreviewGroups',
    'makeSurfacePreviewMaterials',
    'assign_unassigned_material',
    "next==='surfaces'",
):
    if token not in web_sync:
        raise AssertionError(f"SURFACES Web UI contract missing {token!r}")
if 'invalidateWizardFrom("lods");\n    if (!writeWizardState()) sendStatus("Topology class changed' in session_cpp:
    raise AssertionError("surface classification still invalidates the LODS checkpoint")

# 0.10.22: SURFACES analysis is explicit, cached across harmless tab revisits,
# and cross-LOD intent propagation follows stable visual-family identity rather
# than transient G# or coincidental geometry indices. Geometry surface intent
# is the ordinary renderer sidedness authority.
for token in (
    "surfaceAnalysisReady",
    "surfaceAnalysisRequested",
    "wizardSurfaceAnalyzeBtn",
    "wizardSurfaceApplyAllLods",
    "surfaceFamilyKey",
    "surfaceSameFamily",
    "surfaceRenderConsequence",
    "DoubleSide · back-face culling OFF",
    "FrontSide · back-face culling ON",
):
    if token not in web_sync:
        raise AssertionError(f"0.10.22 explicit/cross-LOD SURFACES UI missing {token!r}")
for forbidden in (
    "if(id==='surfaces'&&!state.modelPreflight)send('analyze_model_preflight',{})",
    "if(next==='surfaces')send('analyze_model_preflight',{})",
    "if((id==='geometry'||id==='surfaces')&&previousStage!==id){rebuildScene(true);fitView(false);}",
    "if(next==='geometry'||next==='surfaces'){rebuildScene(true);fitView(false);}",
    "side:(forceDouble||m.twoSided)?THREE.DoubleSide:THREE.FrontSide",
    "id=\"surfaceTwoSided\"",
):
    if forbidden in web_sync:
        raise AssertionError(f"0.10.22 SURFACES retained obsolete automatic/material-sided behavior: {forbidden!r}")
for token in (
    "state.surfaceAnalysisReady=true;rebuildScene(true);fitView(false)",
    "clearLodGeneratorPreview(false,false)",
    "viewportContractChanged",
    "if(next)setWizardStage(next,true)",
):
    if token not in web_sync:
        raise AssertionError(f"0.10.23 wizard stage-entry transaction missing {token!r}")
if "if(next==='geometry'||(next==='surfaces'&&state.surfaceAnalysisReady)){rebuildScene(true);fitView(false);}" in web_sync:
    raise AssertionError("automatic wizard progression still owns a second independent scene-rebuild path")
if "if(id==='geometry'&&previousStage!==id){rebuildScene(true);fitView(false);}" in web_sync:
    raise AssertionError("GEOMETRY tab entry still unconditionally rebuilds the complete scene")

for token in (
    'message.value("applyAllLods", false)',
    "sourceVariantAuthoringId(lodIndex, selectedGeometry)",
    "baseVisualId(lodIndex, selectedGeometry.id)",
    "setGeometryTopologyClass(li, gi, topologyClass, false, false)",
    'sendStatus("Surface intent applied to "',
):
    if token not in session_cpp:
        raise AssertionError(f"0.10.22 cross-LOD surface batch missing {token!r}")

# 0.10.25: explicit surface intent is a metadata-only geometry edit. It must
# not automatically run topology audit, serialize all-asset metadata/material
# usage, send mesh payloads or force a complete Three.js scene rebuild.
for token in (
    "sendSurfaceMetadataPatch",
    '"type", "surface_metadata_patch"',
    "applySurfaceMetadataPatch",
    "applyResidentSurfaceSide",
    "wizard_state_patch",
    "wizard_checkpoint_saved",
    "status==='needs_fix'",
):
    if token not in session_cpp and token not in web_sync:
        raise AssertionError(f"0.10.25 targeted metadata/checkpoint patch missing {token!r}")

set_class_start = session_cpp.index("bool ModelAssetEditorSession::setGeometryTopologyClass(")
set_class_end = session_cpp.index("bool ModelAssetEditorSession::analyzeLodRequirements(", set_class_start)
set_class_body = session_cpp[set_class_start:set_class_end]
if "bool analyzeAfter = false" not in text("tools/model_asset_editor/ModelAssetEditorSession.h"):
    raise AssertionError("surface classification default still requests automatic topology analysis")
if "sendAssetMetadata()" in set_class_body or "sendAsset();" in set_class_body:
    raise AssertionError("surface classification still publishes full asset/geometry payload")

set_cmd_start = session_cpp.index('if (command == "set_geometry_topology_class")')
set_cmd_end = session_cpp.index('if (command == "analyze_lod_requirements")', set_cmd_start)
set_cmd_body = session_cpp[set_cmd_start:set_cmd_end]
if "analyzeModelPreflight()" in set_cmd_body:
    raise AssertionError("explicit/cross-LOD surface classification still auto-runs model preflight")
if "sendAssetMetadata()" in set_cmd_body or "sendAsset();" in set_cmd_body:
    raise AssertionError("surface classification command still retransmits full asset/geometry")

patch_js_start = web_sync.index("function applySurfaceMetadataPatch(msg)")
patch_js_end = web_sync.index("function handle(msg)", patch_js_start)
patch_js_body = web_sync[patch_js_start:patch_js_end]
if "rebuildScene(" in patch_js_body:
    raise AssertionError("targeted surface metadata patch still rebuilds the complete scene")
if "triangleMaterials" in patch_js_body or ".indices" in patch_js_body:
    raise AssertionError("targeted surface metadata patch still traverses triangle geometry")

if "completeDisabled=checkpointCurrent" in web_sync:
    raise AssertionError("current checkpoint still disables explicit re-save / sequence advance")

print("[PASS] model asset editor v0.10.25 metadata-only SURFACES / validation-independent checkpoint SAVE / v4")
