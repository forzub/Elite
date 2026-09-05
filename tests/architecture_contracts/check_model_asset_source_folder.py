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
)

require(
    "tools/model_asset_editor/ModelAssetEditorSession.cpp",
    '{"station", "Orbital Station", ObjectType::Station, "stations"}',
    "sourceFolderAssetAvailable",
    "importSourceFolderAsset",
    "discoverSourceFolderVariants",
    "Legacy assets keep the old registry-assisted discovery",
)

require(
    "CMakeLists.txt",
    "tools/model_asset_editor/SourceFolderImporter.cpp",
)

# The modern path must not need a per-mesh source list. The only registration is
# the asset-level directory. Default geometry comes from direct LOD-root OBJ
# discovery; replacement geometry comes exclusively from variants/.
if "knownRuntimePaths" in source:
    raise AssertionError("modern source importer still carries a registered mesh path list")
if "module.meshes" in source or "assembly.modules" in source:
    raise AssertionError("modern source importer still iterates a C++ assembly mesh list")


# Reimport may intentionally change the declared LOD set. Stale production
# .elmesh files are diagnostics only; LODS validates the current authoring
# snapshot and BUILD/save owns package cleanup.
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
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
web = text("src/assets/webui/model_asset_editor.html")
for required in (
    "staleSaved=payloads.filter",
    "model_editor.wizard.lods.stale_saved",
    "STALE WORKING PAYLOAD · PRUNED ON SAVE",
    "p.declared===false&&Number(p.bytes||0)>0",
):
    if required not in web:
        raise AssertionError(f"missing stale-working LOD UI contract {required!r}")

print("[PASS] model asset folder-authoritative SOURCE / variants boundary / stale working/production LOD cleanup")
