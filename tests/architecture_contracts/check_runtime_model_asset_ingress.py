#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
SCHEMA = (ROOT / "src/model_asset/ModelAsset.h").read_text(encoding="utf-8")
READER = (ROOT / "src/game/assets/CompiledModelAssetReader.cpp").read_text(encoding="utf-8")
LIBRARY = (ROOT / "src/game/assets/RuntimeModelAssetLibrary.cpp").read_text(encoding="utf-8")
ADAPTER = (ROOT / "src/game/assets/LegacyAssemblyModelAdapter.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/game/assets/RuntimeModelAssetLibrary.h").read_text(encoding="utf-8")

assert "constexpr std::uint32_t ModelAssetFormatVersion = 4;" in SCHEMA
for relative in [
    "src/game/assets/CompiledModelAssetReader.h",
    "src/game/assets/CompiledModelAssetReader.cpp",
    "src/game/assets/LegacyAssemblyModelAdapter.h",
    "src/game/assets/LegacyAssemblyModelAdapter.cpp",
    "src/game/assets/RuntimeModelAssetLibrary.h",
    "src/game/assets/RuntimeModelAssetLibrary.cpp",
]:
    source = (ROOT / relative).read_text(encoding="utf-8")
    assert "ModelAssetFormatVersion =" not in source, f"{relative} duplicates format version"
    for token in ("src/render/", "src/ui/", "src/window/", "tools/model_asset_editor", "glad/", "GLFW", "WebView"):
        assert token not in source, f"{relative} crosses runtime asset boundary via {token}"

assert '#include "src/model_asset/ModelAssetBinary.h"' in READER
assert "ModelAssetBinary::load(path, asset" in READER
assert "ModelAssetFormatVersion" in READER
assert "CompiledModelAssetReader::load" in LIBRARY
assert "AssemblyMeshLibrary::get" in LIBRARY
assert "LegacyAssemblyModelAdapter::convert" in LIBRARY
assert "LegacyObjAssembly = 0" in HEADER
assert "CompiledBinary = 1" in HEADER

lib = re.search(r"add_library\(EliteRuntimeModelAssets\s+STATIC\s*(.*?)\n\)", CMAKE, re.S)
assert lib, "missing EliteRuntimeModelAssets"
for source in (
    "src/game/assets/CompiledModelAssetReader.cpp",
    "src/game/assets/LegacyAssemblyModelAdapter.cpp",
    "src/game/assets/RuntimeModelAssetLibrary.cpp",
):
    assert source in lib.group(1), f"EliteRuntimeModelAssets missing {source}"

links = re.search(r"target_link_libraries\(EliteRuntimeModelAssets\s*(.*?)\n\)", CMAKE, re.S)
assert links
assert "EliteModelAsset" in links.group(1)
assert "EliteAssemblyGeometry" in links.group(1)

for target in ("EliteGame", "EliteServer"):
    target_links = re.search(rf"target_link_libraries\({target}\s+PRIVATE\s*(.*?)\n\s*\)", CMAKE, re.S)
    assert target_links and "EliteRuntimeModelAssets" in target_links.group(1), f"{target} does not link runtime model ingress"

assert "PhysicalGeometrySpace::Meters" in ADAPTER
assert "asset.renderLods.push_back" in ADAPTER
assert "asset.nodes.push_back" in ADAPTER

print("RUNTIME MODEL ASSET INGRESS: PASS")
print(" - one shared ModelAsset schema/version authority")
print(" - compiled .elmodel path uses ModelAssetBinary directly")
print(" - legacy OBJ path converges through AssemblyMeshLibrary adapter")
print(" - EliteGame and EliteServer share the same runtime ingress library")
