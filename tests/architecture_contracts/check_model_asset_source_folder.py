#!/usr/bin/env python3
from pathlib import Path

from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def require(path: str, *tokens: str) -> None:
    body = text(path)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{path}: missing {token!r}")


source = text("tools/model_asset_editor/SourceFolderImporter.cpp")
for forbidden in (
    "ObjectAssemblyRegistry",
    "ObjectAssemblyDesc",
    "runtimeAssemblyLodSourcePaths",
):
    if forbidden in source:
        raise AssertionError(
            f"folder-authoritative source importer leaked legacy mesh registration {forbidden!r}"
        )

require(
    "tools/model_asset_editor/SourceFolderImporter.cpp",
    "importSourceFolderAsset",
    "discoverLodFolders",
    "std::filesystem::directory_iterator",
    "directObjFiles",
    "RenderLod lod",
    "RenderGeometryDefinition geometry",
    "RenderNode renderNode",
    "semanticNodeIndex",
    "renderNode.localPosition = glm::vec3(0.0f)",
    'const auto variantsRoot = lodRoot / "variants"',
    "std::filesystem::recursive_directory_iterator",
    "discoverSourceFolderVariants",
    "scanSourceFolderMetadataInventory",
)

session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
for token in (
    '{"station", "Orbital Station", ObjectType::Station, "stations", CatalogSourceAuthority::Folder, CatalogBootstrapMode::Folder}',
    "sourceFolderAssetAvailable",
    "importSourceFolderAsset",
    "discoverSourceFolderVariants",
    "CatalogBootstrapMode::RuntimeAssembly",
    "CatalogSourceAuthority::Folder",
    "selectedSourceAssetRoot",
):
    if token not in session:
        raise AssertionError(f"Folder SOURCE/catalog contract missing {token!r}")

runtime_importer = text("tools/model_asset_editor/RuntimeAssemblyImporter.cpp")
for token in (
    "runtimeLod0FileKeys",
    "appendFolderOnlyLod0Meshes",
    "Runtime descriptors are semantic bootstrap, never a geometry allow-list.",
    "folderOwnsHigherLods",
):
    if token not in runtime_importer:
        raise AssertionError(f"runtime bootstrap/folder geometry split missing {token!r}")

require(
    "CMakeLists.txt",
    "tools/model_asset_editor/SourceFolderImporter.cpp",
)

# The modern folder importer has no per-mesh registration list. Ordinary meshes
# are direct LOD-root OBJ; variants are under variants/ only.
if "knownRuntimePaths" in source:
    raise AssertionError("modern source importer still carries a registered mesh path list")
if "module.meshes" in source or "assembly.modules" in source:
    raise AssertionError("modern source importer still iterates a C++ assembly mesh list")

# Folder import requires contiguous authored LOD directories from LOD0 and loads
# every direct OBJ in each of them into an independent RenderLod.
for token in (
    "source LOD directories must be contiguous from LOD0",
    "for (std::size_t lodIndex = 0; lodIndex < ordinaryFiles.size(); ++lodIndex)",
    "asset.renderLods.push_back(std::move(lod))",
):
    if token not in source:
        raise AssertionError(f"all-authored-LOD import contract missing {token!r}")

# Reimport may intentionally change the declared LOD set. Stale production
# payloads are diagnostics; SAVE/BUILD owns package cleanup.
for forbidden in (
    'saved " + savedPath.filename().string()',
    'current asset declares only " + std::to_string(m_asset.renderLods.size())',
):
    if forbidden in session:
        raise AssertionError("LODS validation still blocks on stale production payloads")
for required in (
    "buildProductionAsset",
    "ModelAssetBinary::pruneStaleLods",
    "They are diagnostics, never LODS blockers",
):
    if required not in session:
        raise AssertionError(f"missing stale-production LOD contract {required!r}")

web = load_source_bundle(ROOT)
if "i.sourceAuthority==='folder'?'SOURCE':'RUNTIME'" not in web:
    raise AssertionError("catalog authority decoration no longer derives from sourceAuthority")
for required in (
    "staleSaved=payloads.filter",
    "model_editor.wizard.lods.stale_saved",
    "STALE WORKING PAYLOAD · PRUNED ON SAVE",
    "p.declared===false&&Number(p.bytes)>0",
):
    if required not in web:
        raise AssertionError(f"missing stale-working LOD UI contract {required!r}")

print("[PASS] model asset folder-authoritative SOURCE / all authored LODs / runtime semantic bootstrap")
