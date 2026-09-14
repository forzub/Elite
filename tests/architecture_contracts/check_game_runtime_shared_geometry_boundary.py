#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
SOURCES = [
    "src/game/geometry/ObjLoader.cpp",
    "src/game/geometry/ObjectAssemblyRegistry.cpp",
    "src/game/geometry/AssemblyMeshLibrary.cpp",
]


def block(pattern: str, label: str) -> str:
    match = re.search(pattern, CMAKE, re.S)
    if not match:
        raise AssertionError(f"missing {label}")
    return match.group(1)


def includes(path: Path) -> list[str]:
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", line)
        if match:
            out.append(match.group(1))
    return out

lib = block(r"add_library\(EliteAssemblyGeometry\s+STATIC\s*(.*?)\n\)", "EliteAssemblyGeometry")
for source in SOURCES:
    assert source in lib, f"{source} is not owned by EliteAssemblyGeometry"

links = block(r"target_link_libraries\(EliteAssemblyGeometry\s+PRIVATE\s*(.*?)\n\)", "EliteAssemblyGeometry links")
assert "EliteModelAsset" in links

for executable in ("EliteGame", "EliteServer"):
    body = block(rf"add_executable\({executable}\s*(.*?)\n\s*\)", f"{executable} sources")
    for source in SOURCES:
        assert source not in body, f"{executable} still compiles {source} directly"
    exe_links = block(rf"target_link_libraries\({executable}\s+PRIVATE\s*(.*?)\n\s*\)", f"{executable} links")
    assert "EliteAssemblyGeometry" in exe_links

editor = block(r"add_executable\(EliteAssetEditor\s*(.*?)\n\s*\)", "EliteAssetEditor sources")
assert "src/game/geometry/ObjectAssemblyRegistry.cpp" in editor

for relative in [
    "src/game/geometry/AssemblyMeshLibrary.h",
    "src/game/geometry/AssemblyMeshLibrary.cpp",
    "src/game/geometry/ObjectAssemblyRegistry.cpp",
    "src/game/geometry/ObjLoader.cpp",
]:
    path = ROOT / relative
    source_text = path.read_text(encoding="utf-8")
    for include in includes(path):
        forbidden_prefixes = (
            "glad/", "GLFW/", "windows.h", "src/ui/", "ui/",
            "src/window/", "window/", "src/game/client/", "game/client/",
            "src/game/server/", "game/server/",
        )
        assert not include.startswith(forbidden_prefixes), (
            f"{relative} crosses CPU boundary via include {include}"
        )
    assert '#include ".cpp"' not in source_text

print("GAME RUNTIME SHARED ASSEMBLY GEOMETRY: PASS")
print(" - EliteAssemblyGeometry owns CPU OBJ hydration + assembly cache")
print(" - EliteGame and EliteServer consume one shared implementation")
print(" - EliteAssetEditor remains an intentionally separate runtime owner")
