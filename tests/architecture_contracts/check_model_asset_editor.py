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

require("tools/model_asset_editor/EditorVersion.h", 'ModelAssetEditorVersion = "0.9.9"')
require(
    "tools/model_asset_editor/CHANGELOG.md",
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
require("src/assets/compiled/models/.gitignore", "Compiled model packages")

cmake = text("CMakeLists.txt")
for token in ("ELITE_BUILD_ASSET_EDITOR", "EliteModelAsset", "NativeObjImporter.cpp", "GeometryInstanceFitter.cpp", "ModelAssetMigration.cpp"):
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
):
    if protected_id not in {c.get("id") for c in capability_doc.get("protected_capabilities", [])}:
        raise AssertionError(f"protected editor capability disappeared: {protected_id}")

print("[PASS] model asset editor v0.9.9 checkpoint resume / geometry finish / recursive extras / stable ids / world XYZ / metadata sync / wizard / v4 boundary")
