#!/usr/bin/env python3
"""Fast structural checks for the map subsystem.

This is intentionally dependency-free so it can run before CMake/Ninja.
It catches the class-boundary mistakes and patch debris that caused several
recent map regressions.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP_DIR = ROOT / "src" / "game" / "system_map"

errors: list[str] = []


def fail(path: Path, message: str) -> None:
    try:
        relative = path.relative_to(ROOT)
    except ValueError:
        relative = path
    errors.append(f"{relative}: {message}")


# Extracted renderers/interactions may only use their explicit view/context.
for pattern in ("*SceneRenderer.*", "*Interaction.*"):
    for path in MAP_DIR.glob(pattern):
        if path.suffix not in {".h", ".cpp", ".inl"}:
            continue
        text = path.read_text(encoding="utf-8")
        for forbidden in ("m_systemView", "m_detailView", "m_hubView"):
            if re.search(rf"\b{forbidden}\b", text):
                fail(path, f"forbidden facade-state reference: {forbidden}")


# Validate the explicit contracts used by extracted scene renderers. This is a
# lightweight compile-contract check that catches misspelled view/context
# members before the full C++ build.
def declared_methods(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    # A permissive name extractor is enough here: false positives merely widen
    # the allowed set, while a misspelled receiver member still fails.
    return set(
        re.findall(
            r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(",
            text,
        )
    )


def check_receiver_contract(
    source: Path,
    receiver: str,
    declaration: Path,
) -> None:
    source_text = source.read_text(encoding="utf-8")
    allowed = declared_methods(declaration)
    for member in sorted(
        set(re.findall(rf"\b{receiver}\.([A-Za-z_][A-Za-z0-9_]*)", source_text))
    ):
        if member not in allowed:
            fail(
                source,
                f"{receiver}.{member} is not declared by {declaration.name}",
            )


check_receiver_contract(
    MAP_DIR / "SystemMapSceneRenderer.inl",
    "viewState",
    MAP_DIR / "SystemMapView.h",
)
check_receiver_contract(
    MAP_DIR / "SystemMapSceneRenderer.inl",
    "context",
    MAP_DIR / "SystemMapRenderContext.h",
)
check_receiver_contract(
    MAP_DIR / "LocalMapInteraction.inl",
    "detailView",
    MAP_DIR / "DetailMapView.h",
)
check_receiver_contract(
    MAP_DIR / "LocalMapInteraction.inl",
    "hubView",
    MAP_DIR / "HubMapView.h",
)


# The shared backend must implement every context operation it inherits.
renderer_header_path = MAP_DIR / "SystemMapRenderer.h"
renderer_header = renderer_header_path.read_text(encoding="utf-8")
for contract_name in (
    "GalaxyMapRenderContext.h",
    "SystemMapRenderContext.h",
    "SystemMapInteraction.h",
    "DetailMapRenderContext.h",
    "HubMapRenderContext.h",
):
    contract_path = MAP_DIR / contract_name
    contract_text = contract_path.read_text(encoding="utf-8")
    pure_virtual_names = set(
        re.findall(
            r"\bvirtual\s+[^;{}]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)[^;{}]*=\s*0\s*;",
            contract_text,
            flags=re.DOTALL,
        )
    )
    for method in sorted(pure_virtual_names):
        if not re.search(
            rf"\b{method}\s*\([^;{{}}]*\)[^;{{}}]*\boverride\b",
            renderer_header,
            flags=re.DOTALL,
        ):
            fail(
                renderer_header_path,
                f"missing override for {contract_name}:{method}",
            )


# Superseded APIs indicate that the old solid-annulus or monolithic body path
# has been reintroduced.
for path in MAP_DIR.glob("*"):
    if path.suffix not in {".h", ".cpp", ".inl"}:
        continue
    text = path.read_text(encoding="utf-8")
    for forbidden in (
        "addRingBand3D",
        "addSystemBodyRingVisuals",
        "addSystemBodyVisual",
    ):
        if re.search(rf"\b{forbidden}\b", text):
            fail(path, f"superseded map-render API: {forbidden}")


# Scene/view/interaction code may not issue OpenGL calls directly. All GL state
# belongs to a render-context backend.
for pattern in ("*SceneRenderer.*", "*Interaction.*", "*View.*"):
    for path in MAP_DIR.glob(pattern):
        if path.suffix not in {".h", ".cpp", ".inl"}:
            continue
        text = path.read_text(encoding="utf-8")
        if re.search(r"\bgl[A-Z][A-Za-z0-9_]*\s*\(", text):
            fail(path, "OpenGL call outside a render-context backend")


# Regression guards for the helper/API breakages that previously compiled only
# after stacking corrective patches.
renderer_cpp = (MAP_DIR / "SystemMapRenderer.cpp").read_text(encoding="utf-8")
for helper in (
    "planetEastAxisWorld",
    "systemBodyNorthAxisWorld",
    "systemBodyPrimeAxisWorld",
    "systemBodyRingAxisYWorld",
):
    definitions = len(
        re.findall(
            rf"\bglm::dvec3\s+{helper}\s*\(",
            renderer_cpp,
        )
    )
    if definitions != 1:
        fail(
            MAP_DIR / "SystemMapRenderer.cpp",
            f"expected exactly one {helper} definition, found {definitions}",
        )


# C++ and GLSL must agree on the shared ring-pattern uniform.
ring_cpp = (
    ROOT / "src" / "render" / "celestial" / "rings" /
    "PlanetRingRenderer.cpp"
).read_text(encoding="utf-8")
ring_shader = (
    ROOT / "src" / "assets" / "shaders" / "celestial" /
    "planet_rings.frag"
).read_text(encoding="utf-8")
if '"uPatternPhase"' not in ring_cpp:
    fail(
        ROOT / "src" / "render" / "celestial" / "rings" /
        "PlanetRingRenderer.cpp",
        "missing uPatternPhase uniform lookup",
    )
if not re.search(r"\buniform\s+float\s+uPatternPhase\s*;", ring_shader):
    fail(
        ROOT / "src" / "assets" / "shaders" / "celestial" /
        "planet_rings.frag",
        "missing uPatternPhase uniform declaration",
    )
if '"uOpacityScale"' not in ring_cpp:
    fail(
        ROOT / "src" / "render" / "celestial" / "rings" /
        "PlanetRingRenderer.cpp",
        "missing uOpacityScale uniform lookup",
    )
if not re.search(r"\buniform\s+float\s+uOpacityScale\s*;", ring_shader):
    fail(
        ROOT / "src" / "assets" / "shaders" / "celestial" /
        "planet_rings.frag",
        "missing uOpacityScale uniform declaration",
    )


# Deprecated mode aliases must not creep back into the source tree.
for path in (ROOT / "src").rglob("*"):
    if not path.is_file() or path.suffix not in {".h", ".cpp", ".inl"}:
        continue
    text = path.read_text(encoding="utf-8", errors="replace")
    if re.search(r"\b(?:MapMode|Mode)::Planet\b", text):
        fail(path, "deprecated Planet map-mode alias")


# Patch rejects/backups must never become source inputs or library artifacts.
for suffix in ("*.orig", "*.rej"):
    for path in (ROOT / "src").rglob(suffix):
        fail(path, "patch artifact committed under src/")


# Source hygiene for files owned by the map subsystem and shared ring renderer.
checked_roots = (
    MAP_DIR,
    ROOT / "src" / "render" / "celestial" / "rings",
)
for checked_root in checked_roots:
    for path in checked_root.rglob("*"):
        if not path.is_file() or path.suffix not in {
            ".h", ".hpp", ".cpp", ".inl", ".frag", ".vert", ".md"
        }:
            continue

        data = path.read_bytes()

        if data and not data.endswith(b"\n"):
            fail(path, "missing newline at end of file")

        for line_number, line in enumerate(data.splitlines(), start=1):
            if line.rstrip(b" \t") != line:
                fail(path, f"trailing whitespace on line {line_number}")


ring_shader_path = (
    ROOT / "src" / "assets" / "shaders" / "celestial" /
    "planet_rings.frag"
)
ring_shader_data = ring_shader_path.read_bytes()
if ring_shader_data and not ring_shader_data.endswith(b"\n"):
    fail(ring_shader_path, "missing newline at end of file")
for line_number, line in enumerate(ring_shader_data.splitlines(), start=1):
    if line.rstrip(b" \t") != line:
        fail(ring_shader_path, f"trailing whitespace on line {line_number}")


if errors:
    print("Map architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Map architecture check passed.")
