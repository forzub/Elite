#pragma once

#include <string>

namespace render::gpu
{

struct GlRuntimeCapabilities
{
    int contextMajor = 0;
    int contextMinor = 0;
    int gladMajor = 0;
    int gladMinor = 0;
    bool coreProfile = false;
    bool computeShaders = false;
    bool shaderStorageBuffers = false;
    int maxComputeWorkGroupInvocations = 0;
    int maxComputeSharedMemoryBytes = 0;
    int maxShaderStorageBufferBindings = 0;
    std::string vendor;
    std::string renderer;
    std::string version;
    std::string shadingLanguageVersion;

    bool meetsOpenGl43Baseline() const;
};

GlRuntimeCapabilities queryGlRuntimeCapabilities(int gladLoadedVersion);
void requireOpenGl43Baseline(int gladLoadedVersion);

} // namespace render::gpu
