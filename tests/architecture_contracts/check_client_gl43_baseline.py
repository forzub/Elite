#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def text(path):
    return (ROOT / path).read_text(encoding='utf-8')

window = text('src/window/Window.cpp')
glad = text('glad/include/glad/gl.h')
flags = text('src/game/RuntimeFeatureFlags.h')
space = text('src/game/SpaceState.cpp')
cmake = text('CMakeLists.txt')
cap_h = text('src/render/gpu/GlRuntimeCapabilities.h')
cap_cpp = text('src/render/gpu/GlRuntimeCapabilities.cpp')
plan = text('src/render/GPU_OFFLOAD_PLAN.md')

assert 'GLFW_CONTEXT_VERSION_MAJOR, 4' in window
assert 'GLFW_CONTEXT_VERSION_MINOR, 3' in window
assert 'GLFW_OPENGL_COMPAT_PROFILE' in window
assert 'GLFW_OPENGL_CORE_PROFILE' not in window
assert 'requireOpenGl43Baseline(gladVersion)' in window

assert "gl:compatibility=4.3" in glad, 'bundled GLAD is not generated for OpenGL 4.3 compatibility'
assert 'GLAD_GL_VERSION_4_3' in glad
assert 'glad_glDispatchCompute' in glad
assert 'GL_SHADER_STORAGE_BUFFER' in glad
assert 'GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS' in glad

assert 'src/render/gpu/GlRuntimeCapabilities.cpp' in cmake
assert 'GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS' in cap_cpp
assert 'GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS' in cap_cpp
assert 'computeShaders' in cap_h and 'shaderStorageBuffers' in cap_h

assert 'inline constexpr bool WorldSignalLabelsEnabled = false;' in flags
call = space.index('m_playerView->renderWorldLabels(')
assert 'if (game::runtime::WorldSignalLabelsEnabled)' in space[max(0, call - 300):call]
feed = space.index('ship.signalPresentation.labelsVector()')
assert 'game::runtime::WorldSignalLabelsEnabled' in space[max(0, feed - 300):feed]

# Required close-navigation labels remain physically present and built.
assert 'src/render/navigation/NavigationCellLabelLayer.cpp' in cmake
assert 'src/render/system_map/HubMapGpuGeometryRenderer.cpp' in cmake
assert 'src/render/HUD/WorldLabelRenderer.cpp' in cmake  # navigation markers still share it
assert 'near-navigation labels' in plan

print('CLIENT OPENGL 4.3 GPU BASELINE: PASS')
print(' - GLFW requires OpenGL 4.3 compatibility during legacy-render transition')
print(' - bundled GLAD exposes core 4.3 compute/SSBO entry points')
print(' - centralized startup capability gate owns the supported GPU floor')
print(' - general world-signal labels are disabled; Hub/near-navigation labels remain')
