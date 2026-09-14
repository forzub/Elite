#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
helper = (ROOT / "src/game/system_map/SystemMapGpuCircleBatch.h").read_text(encoding="utf-8")
context = (ROOT / "src/game/system_map/SystemMapRenderContext.h").read_text(encoding="utf-8")
renderer = (ROOT / "src/game/system_map/SystemMapRenderer.h").read_text(encoding="utf-8")
scene = (ROOT / "src/game/system_map/SystemMapSceneRenderer.cpp").read_text(encoding="utf-8")

for token in (
    "#version 430 core",
    "GL_STATIC_DRAW",
    "glVertexAttribDivisor",
    "glDrawArraysInstanced",
    "GL_LINE_LOOP",
    "ensureMesh(batch.segments)",
    "iCenterRadius",
    "iColor",
):
    assert token in helper, token

# Trigonometry is allowed only in the lazy resident unit-mesh construction.
mesh_start = helper.index("Mesh& ensureMesh(int segments)")
mesh_body = helper[mesh_start:]
prefix = helper[:mesh_start]
assert "std::cos" not in prefix
assert "std::sin" not in prefix
assert "std::cos" in mesh_body
assert "std::sin" in mesh_body
assert "GL_STATIC_DRAW" in mesh_body

for token in (
    "beginGpuCircles",
    "addGpuCircleXZ",
    "addGpuCircleXY",
    "flushGpuCircles",
):
    assert token in context, token
    assert token in renderer, token
    assert token in scene, token

# Scene-level repeated planar circles must not return to the CPU tessellation API.
for retired in (
    "context.addCircleXZ(",
    "context.addCircleXY(",
):
    assert retired not in scene, retired

# The migrated scene should only submit compact circle instances; there must be
# no per-frame circle sin/cos loop in the orchestration layer.
assert "std::cos(" not in scene
assert "std::sin(" not in scene

# Guard the important visible consumers that make this migration worthwhile.
for token in (
    "primaryOrbitSegments",
    "moonOrbitSegments",
    "selectedRingColor",
    "selectedSecondaryRingColor",
    "selectedHubRingColor",
    "selectedHubSecondaryRingColor",
):
    assert token in scene, token

print("SYSTEM MAP GPU CIRCLES: PASS")
print(" - repeated planar rings use resident GL_STATIC_DRAW unit-circle meshes")
print(" - center/radius/color are streamed as per-instance data")
print(" - migrated circles are submitted with glDrawArraysInstanced")
print(" - SystemMapSceneRenderer no longer CPU-tessellates XZ/XY rings each frame")
print(" - arbitrary lines and non-planar/billboard marker geometry remain separate migration debt")
