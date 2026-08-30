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
    "loadLodOnly",
    "unloadLod",
    "complete_wizard_stage",
    "restore_wizard_checkpoint",
    "scan_render_duplicates",
    "consolidate_render_duplicates",
    "wizardCheckpointPath",
    "pruneWizardAfter",
    "sendAssetMetadata",
    "serializeAsset(false)",
    "geometryPayloadIncluded",
    "ModelAssetBinary::validate",
    "preflight failed",
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

require("tools/model_asset_editor/EditorVersion.h", 'ModelAssetEditorVersion = "0.10.16"')
require(
    "tools/model_asset_editor/CHANGELOG.md",
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
    "model_asset_mesh_repair.log",
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
if "SOURCE BLOCKED: raw mesh was not exposed" in canonical_body:
    raise AssertionError("canonical preparation still masquerades as a SOURCE load blocker")

verify_start = session.index("bool ModelAssetEditorSession::verifyLoadedWorkingSetCanonical(")
verify_end = session.index("bool ModelAssetEditorSession::setGeometryTopologyClass(", verify_start)
verify_body = session[verify_start:verify_end]
if "structuralInvalid) continue" in verify_body:
    raise AssertionError("canonical verification lets invalid payloads bypass downstream records")

send_start = session.index("void ModelAssetEditorSession::sendAsset()")
send_end = session.index("void ModelAssetEditorSession::sendAssetMetadata", send_start)
send_body = session[send_start:send_end]
if "verifyLoadedWorkingSetCanonical" in send_body or "ASSET PAYLOAD BLOCKED" in send_body:
    raise AssertionError("sendAsset regressed into a load-time canonical gate")
if "serializeAsset(true)" not in send_body:
    raise AssertionError("sendAsset no longer publishes the current resident working mesh")

select_start = session.index("bool ModelAssetEditorSession::selectAsset(")
select_end = session.index("bool ModelAssetEditorSession::saveAsset(", select_start)
select_body = session[select_start:select_end]
if "canonicalizeLoadedWorkingSet(" in select_body:
    raise AssertionError("selectAsset must load/restore/reimport without hidden canonicalization")
if "refreshSourceVariants(true, false)" not in select_body or "sendAsset();" not in select_body:
    raise AssertionError("selectAsset no longer materializes the source set and publishes it as-is")

restore_start = session.index("bool ModelAssetEditorSession::restoreWizardCheckpoint(")
restore_end = session.index("bool ModelAssetEditorSession::scanRenderDuplicates(", restore_start)
restore_body = session[restore_start:restore_end]
if "canonicalizeLoadedWorkingSet(" in restore_body:
    raise AssertionError("checkpoint restore must not canonicalize implicitly")
if "sendAsset();" not in restore_body:
    raise AssertionError("checkpoint restore must publish exactly the restored payload")

load_lod_start = session.index("bool ModelAssetEditorSession::loadLodOnly(")
load_lod_end = session.index("bool ModelAssetEditorSession::ensureLodLoaded(", load_lod_start)
if "canonicalizeLoadedWorkingSet(" in session[load_lod_start:load_lod_end]:
    raise AssertionError("manual LOD load/reload must not canonicalize implicitly")

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

require("src/assets/compiled/models/.gitignore", "Compiled model packages")

cmake = text("CMakeLists.txt")
for token in ("ELITE_BUILD_ASSET_EDITOR", "EliteModelAsset", "NativeObjImporter.cpp", "CanonicalMeshBuilder.cpp", "GeometryInstanceFitter.cpp", "ModelAssetMigration.cpp"):
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
    'serializeAsset(bool includeGeometryPayload)',
    'if (includeGeometryPayload)',
    '"type", "asset_metadata"',
    'serializeAsset(false)',
    'std::filesystem::remove_all(wizardCheckpointPath(laterId).parent_path()',
    'latestWizardCheckpoint',
    '"RESUME CHECKPOINT"',
    'ModelAssetBinary::load(resumeCheckpoint.string()',
    'Production package was not loaded instead.',
):
    if token not in session_cpp:
        raise AssertionError(f"metadata/checkpoint architecture missing {token!r}")

resume_branch = session_cpp.find("if (resumeWorkspace)")
compiled_branch = session_cpp.find("else if (!forceReimport && (havePackage || haveLegacyV2))")
if resume_branch < 0 or compiled_branch < 0 or resume_branch > compiled_branch:
    raise AssertionError("asset selection no longer resumes wizard checkpoint before production package")

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
    "set_surface_mode",
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
    '"schemaVersion", 6',
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

# LOD generator v1 is deliberately preview-only. It must use the fixed project
# authoring ceiling, analyze component thickness rather than filename/triangle
# count heuristics, and reuse resident browser mesh payloads via removal ranges.
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
):
    if token not in web_sync:
        raise AssertionError(f"LOD generator Web UI contract missing {token!r}")

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
    "model_preflight",
):
    if protected_id not in {c.get("id") for c in capability_doc.get("protected_capabilities", [])}:
        raise AssertionError(f"protected editor capability disappeared: {protected_id}")

print("[PASS] model asset editor v0.10.16 libigl + Embree canonical preparation / diagnostic viewport / v4")
