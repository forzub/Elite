#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / "src/game/system_map/SystemMapRenderer.h").read_text(encoding="utf-8")
cpp = (ROOT / "src/game/system_map/SystemMapRenderer.cpp").read_text(encoding="utf-8")
inl = (ROOT / "src/game/system_map/SystemMapRendererSystem.inl").read_text(encoding="utf-8")
vert = (ROOT / "src/assets/shaders/system_map/map_body_preview.vert").read_text(encoding="utf-8")

assert "TexturedSphereGpuMesh" in header
assert "TexturedBodyDraw" in header
assert "m_texturedSphereLow" in header
assert "m_texturedSphereHigh" in header

create_start = cpp.index("void SystemMapRenderer::createTexturedSphereMesh(")
create_end = cpp.index("void SystemMapRenderer::ensureTexturedGlObjects()", create_start)
create_body = cpp[create_start:create_end]
assert "GL_STATIC_DRAW" in create_body
assert "glDraw" not in create_body
assert "64" in cpp[create_end:cpp.index("void SystemMapRenderer::ensureShader()", create_end)]
assert "128" in cpp[create_end:cpp.index("void SystemMapRenderer::ensureShader()", create_end)]

flush_start = cpp.index("void SystemMapRenderer::flushTexturedBodies(")
flush_end = cpp.index("void SystemMapRenderer::addCross(", flush_start)
flush_body = cpp[flush_start:flush_end]
assert "glDrawElements" in flush_body
assert "GL_DYNAMIC_DRAW" not in flush_body
assert "glBufferData" not in flush_body
assert "batch.vertices" not in flush_body

sphere_start = inl.index("void SystemMapRenderer::addTexturedSystemBodySphere(")
sphere_end = inl.index("// ============================================================================\n// System body visual metrics", sphere_start)
sphere_body = inl[sphere_start:sphere_end]
assert "bodyPoint" not in sphere_body
assert "for (int iy" not in sphere_body
assert "for (int ix" not in sphere_body
assert "rotationPhaseRad" in sphere_body
assert "textureLongitudeOffsetDeg" in sphere_body
assert "batch->bodies.push_back" in sphere_body

for token in (
    "#version 430 core",
    "aUnitPosition",
    "uBodyCenter",
    "uBodyRadius",
    "uPrimeAxis",
    "uNorthAxis",
    "uEastAxis",
    "uColor",
):
    assert token in vert, token

print("SYSTEM MAP STATIC TEXTURED SPHERE: PASS")
print(" - 24x48 and 64x128 indexed sphere meshes are one-time GL_STATIC_DRAW resources")
print(" - per-frame textured body path records only body parameters")
print(" - draw path uses glDrawElements and performs no dynamic full-sphere upload")
print(" - rotation phase / longitude offset are folded into the per-body basis")
