#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

SOURCES = [
    "src/world/navigation/NavigationObstacleGeometry.cpp",
    "src/world/navigation/GeometricPathPlanner.cpp",
]


def block(pattern: str, label: str) -> str:
    match = re.search(pattern, CMAKE, re.S)
    if not match:
        raise AssertionError(f"missing {label}")
    return match.group(1)


lib = block(
    r"add_library\(EliteNavigationGeometry\s+STATIC\s*(.*?)\n\)",
    "EliteNavigationGeometry library",
)
for source in SOURCES:
    assert source in lib, f"{source} is not owned by EliteNavigationGeometry"

for executable in ("EliteGame", "EliteServer"):
    body = block(
        rf"add_executable\({executable}\s*(.*?)\n\s*\)",
        f"{executable} source list",
    )
    for source in SOURCES:
        assert source not in body, f"{executable} still compiles {source} directly"

    links = block(
        rf"target_link_libraries\({executable}\s+PRIVATE\s*(.*?)\n\s*\)",
        f"{executable} link list",
    )
    assert "EliteNavigationGeometry" in links, (
        f"{executable} does not link EliteNavigationGeometry"
    )

for relative in SOURCES + [
    "src/world/navigation/NavigationObstacleGeometry.h",
    "src/world/navigation/GeometricPathPlanner.h",
]:
    source_text = (ROOT / relative).read_text(encoding="utf-8")
    forbidden = (
        "src/render/", "src/ui/", "src/window/", "src/game/client/",
        "src/game/server/", "GLFW", "glad/", "<windows.h>", "WebView",
    )
    for token in forbidden:
        assert token not in source_text, f"{relative} crosses boundary via {token}"
    assert '#include ".cpp"' not in source_text

print("GAME RUNTIME LIBRARY BOUNDARY R0: PASS")
print(" - EliteNavigationGeometry owns obstacle geometry + geometric path planning")
print(" - EliteGame and EliteServer link the library instead of recompiling sources")
print(" - seed navigation core has no render/UI/client/server/platform dependency")
