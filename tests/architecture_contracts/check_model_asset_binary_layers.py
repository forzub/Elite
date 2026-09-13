#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "src/model_asset/binary"
FACADE = ROOT / "src/model_asset/ModelAssetBinary.cpp"
MODEL = ROOT / "src/model_asset/ModelAsset.h"
ROOT_CMAKE = ROOT / "CMakeLists.txt"
TEST_CMAKE = ROOT / "tests/model_asset/CMakeLists.txt"

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

# Translation-unit closure: implementation files must compile independently.
all_binary_cpp = [BINARY / rel for rel in EXPECTED if rel.endswith(".cpp")]
for path in [FACADE, *all_binary_cpp]:
    text = path.read_text(encoding="utf-8")
    assert not re.search(r'#\s*include\s*[<"][^>"]+\.cpp[>"]', text), f".cpp aggregation include forbidden: {path.relative_to(ROOT)}"

root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")
test_cmake = TEST_CMAKE.read_text(encoding="utf-8")
root_target = re.search(r'add_library\(EliteModelAsset\s+STATIC(?P<body>.*?)\n\)', root_cmake, re.S)
assert root_target, "EliteModelAsset STATIC target not found"
root_body = root_target.group("body")
test_target = re.search(r'add_executable\(model_asset_tests(?P<body>.*?)\n\)', test_cmake, re.S)
assert test_target, "model_asset_tests target not found"
test_body = test_target.group("body")
for rel in ["ModelAssetBinary.cpp", *[str(path.relative_to(ROOT / "src/model_asset")) for path in all_binary_cpp]]:
    source = f"src/model_asset/{rel}"
    assert source in root_body, f"EliteModelAsset does not compile independent TU: {source}"
    assert root_body.count(source) == 1, f"EliteModelAsset source duplicated: {source}"
    assert source in test_body, f"model_asset_tests does not exercise independent TU: {source}"

print("MODEL ASSET BINARY LAYERS: PASS")
print(" - public facade delegates only to controller")
print(" - controller owns orchestration, not wire/chunk details")
print(" - manifest, LOD, validation and storage layers are isolated")
print(" - manifest codecs are split into eight domain files")
print(" - every binary implementation is an independent CMake translation unit")
print(" - .cpp aggregation includes are forbidden")
print(" - production format remains v4")
