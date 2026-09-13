#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "src/model_asset/binary"
FACADE = ROOT / "src/model_asset/ModelAssetBinary.cpp"
MODEL = ROOT / "src/model_asset/ModelAsset.h"

EXPECTED = [
    "ModelAssetBinaryWire.h",
    "ModelAssetBinaryMeshCodec.h",
    "ModelAssetBinaryMeshCodec.cpp",
    "ModelAssetBinaryChunkCodecs.h",
    "ModelAssetBinaryChunkRegistry.cpp",
    "ModelAssetBinaryValidation.h",
    "ModelAssetBinaryValidation.cpp",
    "ModelAssetBinaryStorage.h",
    "ModelAssetBinaryStorage.cpp",
    "ModelAssetBinaryManifestIO.h",
    "ModelAssetBinaryManifestIO.cpp",
    "ModelAssetBinaryLodIO.h",
    "ModelAssetBinaryLodIO.cpp",
    "ModelAssetBinaryController.h",
    "ModelAssetBinaryController.cpp",
    "chunks/MetadataChunks.cpp",
    "chunks/SemanticsChunks.cpp",
    "chunks/CollisionChunks.cpp",
    "chunks/SocketChunks.cpp",
    "chunks/DamageChunks.cpp",
    "chunks/StructuralChunks.cpp",
    "chunks/LodChunks.cpp",
    "chunks/LegacyChunks.cpp",
]

for rel in EXPECTED:
    path = BINARY / rel
    assert path.is_file(), f"missing binary layer: {rel}"

facade = FACADE.read_text(encoding="utf-8")
assert "binary::controller::" in facade
for forbidden in ["std::ofstream", "std::ifstream", "struct Writer", "struct Reader", "ManifestMagicV4", "writeMeshLod("]:
    assert forbidden not in facade, f"public facade regained implementation detail: {forbidden}"

controller = (BINARY / "ModelAssetBinaryController.cpp").read_text(encoding="utf-8")
for required in ["saveManifest(", "saveLod(", "pruneStaleLods(", "buildIndependentRenderLodsFromLegacy", "Manifest remains the package commit point"]:
    assert required in controller, f"controller lost orchestration responsibility: {required}"
for forbidden in ["ManifestMagic", "MeshMagic", "ChunkSpec", "writeSemanticNodesV4", "writeMeshLod(", "std::ofstream", "std::ifstream"]:
    assert forbidden not in controller, f"controller leaked lower-layer implementation: {forbidden}"

validation = (BINARY / "ModelAssetBinaryValidation.cpp").read_text(encoding="utf-8")
for forbidden in ["std::ofstream", "std::ifstream", "ManifestMagic", "MeshMagic", "ChunkSpec", "packageLodPayloadPath"]:
    assert forbidden not in validation, f"validation leaked I/O/storage responsibility: {forbidden}"

storage = (BINARY / "ModelAssetBinaryStorage.cpp").read_text(encoding="utf-8")
for forbidden in ["Writer", "Reader", "ChunkSpec", "ManifestMagic", "MeshMagic", "validateSemanticAsset"]:
    assert forbidden not in storage, f"storage leaked codec/validation responsibility: {forbidden}"

manifest_io = (BINARY / "ModelAssetBinaryManifestIO.cpp").read_text(encoding="utf-8")
assert "findManifestChunk" in manifest_io and "manifestChunksV4" in manifest_io
for forbidden in ["validateSemanticAsset", "buildIndependentRenderLodsFromLegacy", "packageLodPayloadPath"]:
    assert forbidden not in manifest_io, f"manifest I/O leaked controller responsibility: {forbidden}"

lod_io = (BINARY / "ModelAssetBinaryLodIO.cpp").read_text(encoding="utf-8")
assert "writeMeshLod" in lod_io and "readMeshLod" in lod_io
for forbidden in ["buildIndependentRenderLodsFromLegacy", "manifestChunksV4", "writeSemanticNodesV4"]:
    assert forbidden not in lod_io, f"LOD I/O leaked unrelated responsibility: {forbidden}"

chunk_files = list((BINARY / "chunks").glob("*.cpp"))
assert len(chunk_files) == 8, f"expected eight domain codec files, found {len(chunk_files)}"
for path in chunk_files:
    text = path.read_text(encoding="utf-8")
    for forbidden in ["ModelAssetBinaryController", "std::ofstream", "std::ifstream", "packageLodPayloadPath"]:
        assert forbidden not in text, f"{path.name} leaked upward dependency: {forbidden}"

registry = (BINARY / "ModelAssetBinaryChunkRegistry.cpp").read_text(encoding="utf-8")
for fourcc in ["M','E','T','A", "S','E','M','N", "S','T','R','L", "L','O','D','S"]:
    assert fourcc in registry, f"chunk registry missing {fourcc}"

model = MODEL.read_text(encoding="utf-8")
assert re.search(r"ModelAssetFormatVersion\s*=\s*4\s*;", model), "production binary version changed during layer split"

# CMake still names only ModelAssetBinary.cpp; until target_sources is split,
# the facade explicitly aggregates the layer .cpp files so this refactor cannot
# leave the branch with unresolved symbols. This is a deliberate temporary
# composition boundary, not permission to move logic back into the facade.
for rel in [
    "ModelAssetBinaryController.cpp",
    "ModelAssetBinaryManifestIO.cpp",
    "ModelAssetBinaryLodIO.cpp",
    "ModelAssetBinaryValidation.cpp",
]:
    assert rel in facade, f"temporary composition TU does not include {rel}"

print("MODEL ASSET BINARY LAYERS: PASS")
print(" - public facade delegates only to controller")
print(" - controller owns orchestration, not wire/chunk details")
print(" - manifest, LOD, validation and storage layers are isolated")
print(" - manifest codecs are split into eight domain files")
print(" - production format remains v4")
