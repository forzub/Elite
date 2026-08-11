#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"Headless-server architecture check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


cpu_assembly_h = read("src/game/geometry/ObjectAssembly.h")
cpu_library_h = read("src/game/geometry/AssemblyMeshLibrary.h")
cpu_library_cpp = read("src/game/geometry/AssemblyMeshLibrary.cpp")
gpu_library_h = read("src/render/geometry/AssemblyGpuLibrary.h")
gpu_library_cpp = read("src/render/geometry/AssemblyGpuLibrary.cpp")
scene_renderer = read("src/scene/SceneRenderer.cpp")
hub_map_pass = read("src/game/system_map/HubMapGeometryPass.cpp")

# ObjectAssembly is shared local CPU data. Authoritative/headless code must be
# able to include it without bringing an OpenGL type into the compilation unit.
for token in (
    "MeshGPU",
    "glad/gl.h",
    "GLuint",
    "gpuReady",
    "lod0Gpu",
    "lod1Gpu",
    "wholeShipProxyGpu",
):
    if token in cpu_assembly_h:
        fail(f"CPU ObjectAssembly owns presentation/GPU state again: {token}")

for text, label in (
    (cpu_library_h, "AssemblyMeshLibrary.h"),
    (cpu_library_cpp, "AssemblyMeshLibrary.cpp"),
):
    for token in (
        "MeshGPU",
        "glad/gl.h",
        "getGpuReady",
        "uploadGpu",
    ):
        if token in text:
            fail(f"{label} regained a GPU/OpenGL responsibility: {token}")

for token in (
    "class AssemblyGpuLibrary",
    "ObjectAssemblyGpuResources",
):
    if token not in gpu_library_h:
        fail(f"render-side assembly GPU seam missing: {token}")

for token in (
    "AssemblyMeshLibrary::get(typeId)",
    "gpuPart.lod0.upload(cpuPart.lod0Mesh)",
    "gpuPart.lod1.upload(cpuPart.lod1Mesh)",
):
    if token not in gpu_library_cpp:
        fail(f"render-side GPU sidecar no longer derives from shared CPU data: {token}")

for text, label in (
    (scene_renderer, "SceneRenderer.cpp"),
    (hub_map_pass, "HubMapGeometryPass.cpp"),
):
    if "AssemblyMeshLibrary::getGpuReady" in text:
        fail(f"{label} still asks the shared CPU library to create GPU resources")

print("Headless-server assembly boundary check passed.")
