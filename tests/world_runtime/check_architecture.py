#!/usr/bin/env python3

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors: list[str] = []


def fail(path: Path, message: str) -> None:
    try:
        shown = path.relative_to(ROOT)
    except ValueError:
        shown = path
    errors.append(f"{shown}: {message}")


for path in (
    SRC / "game/network/PresentationDataMessage.h",
    SRC / "game/client/ClientCatalogService.h",
    SRC / "game/client/ClientCatalogService.cpp",
    SRC / "game/client/GameClient.cpp",
    SRC / "game/server/GameServer.cpp",
):
    if not path.is_file():
        fail(path, "required source file is missing")
        continue

    text = path.read_text(encoding="utf-8", errors="replace")

    for forbidden in (
        "CelestialSnapshotRequest",
        "CelestialSnapshotResponse",
        "sendCelestialRequest",
        "m_celestialRefreshAccumulator",
    ):
        if forbidden in text:
            fail(path, f"obsolete celestial streaming path returned: {forbidden}")

registry_header = SRC / "world/celestial/CelestialRuntimeRegistry.h"
registry_cpp = SRC / "world/celestial/CelestialRuntimeRegistry.cpp"

for path in (registry_header, registry_cpp):
    if not path.is_file():
        fail(path, "shared demand-driven celestial resolver is missing")

if registry_header.is_file():
    text = registry_header.read_text(encoding="utf-8", errors="replace")

    for required in (
        "const CelestialSystemSnapshot* resolve(",
        "double universeTimeSeconds",
        "cachedSystemCount()",
        "const StarAtlasDatabase* m_atlas",
    ):
        if required not in text:
            fail(registry_header, f"resolver contract is incomplete: {required}")

    if "void update(double universeTimeSeconds)" in text:
        fail(registry_header, "registry must not update every cached system per tick")

catalog_cpp = SRC / "game/client/ClientCatalogService.cpp"
if catalog_cpp.is_file():
    text = catalog_cpp.read_text(encoding="utf-8", errors="replace")

    for required in (
        "m_celestialRuntimes.initialize(m_starAtlas)",
        "m_celestialRuntimes.resolve(",
    ):
        if required not in text:
            fail(catalog_cpp, f"client local celestial reconstruction is missing: {required}")

client_cpp = SRC / "game/client/GameClient.cpp"
if client_cpp.is_file():
    text = client_cpp.read_text(encoding="utf-8", errors="replace")

    for required in (
        "m_universeClock.synchronize(",
        "m_universeClock.advance(",
        "m_catalogs.resolveCelestialSnapshot(",
    ):
        if required not in text:
            fail(client_cpp, f"client universe-time pipeline is incomplete: {required}")

server_cpp = SRC / "game/server/GameServer.cpp"
if server_cpp.is_file():
    text = server_cpp.read_text(encoding="utf-8", errors="replace")

    if "m_celestialRuntimes.update(" in text:
        fail(server_cpp, "server still evaluates every celestial runtime each tick")

    if "m_celestialRuntimes.resolve(" not in text:
        fail(server_cpp, "server does not use the shared demand-driven resolver")

if errors:
    print("World runtime architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("World runtime architecture check passed.")
