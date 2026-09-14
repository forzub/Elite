#include "src/render/gpu/GlRuntimeCapabilities.h"

#include <glad/gl.h>

#include <iostream>
#include <sstream>
#include <stdexcept>

namespace render::gpu
{
namespace
{
std::string glString(GLenum name)
{
    const auto* value = glGetString(name);
    return value ? reinterpret_cast<const char*>(value) : std::string();
}

bool atLeast(int major, int minor, int requiredMajor, int requiredMinor)
{
    return major > requiredMajor ||
        (major == requiredMajor && minor >= requiredMinor);
}
} // namespace

bool GlRuntimeCapabilities::meetsOpenGl43Baseline() const
{
    return atLeast(contextMajor, contextMinor, 4, 3) &&
        atLeast(gladMajor, gladMinor, 4, 3) &&
        compatibilityProfile &&
        computeShaders &&
        shaderStorageBuffers;
}

GlRuntimeCapabilities queryGlRuntimeCapabilities(int gladLoadedVersion)
{
    GlRuntimeCapabilities caps;

    glGetIntegerv(GL_MAJOR_VERSION, &caps.contextMajor);
    glGetIntegerv(GL_MINOR_VERSION, &caps.contextMinor);
    caps.gladMajor = GLAD_VERSION_MAJOR(gladLoadedVersion);
    caps.gladMinor = GLAD_VERSION_MINOR(gladLoadedVersion);

    GLint profileMask = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
    caps.compatibilityProfile =
        (profileMask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0;

    caps.vendor = glString(GL_VENDOR);
    caps.renderer = glString(GL_RENDERER);
    caps.version = glString(GL_VERSION);
    caps.shadingLanguageVersion = glString(GL_SHADING_LANGUAGE_VERSION);

    caps.computeShaders =
        GLAD_GL_VERSION_4_3 != 0 &&
        glad_glDispatchCompute != nullptr &&
        glad_glMemoryBarrier != nullptr;

    caps.shaderStorageBuffers =
        GLAD_GL_VERSION_4_3 != 0 &&
        glad_glBindBufferBase != nullptr;

    if (GLAD_GL_VERSION_4_3)
    {
        glGetIntegerv(
            GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
            &caps.maxComputeWorkGroupInvocations
        );
        glGetIntegerv(
            GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,
            &caps.maxComputeSharedMemoryBytes
        );
        glGetIntegerv(
            GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
            &caps.maxShaderStorageBufferBindings
        );
    }

    return caps;
}

void requireOpenGl43Baseline(int gladLoadedVersion)
{
    const GlRuntimeCapabilities caps =
        queryGlRuntimeCapabilities(gladLoadedVersion);

    std::cout
        << "[OpenGL] context=" << caps.contextMajor << '.' << caps.contextMinor
        << " glad=" << caps.gladMajor << '.' << caps.gladMinor
        << " profile=" << (caps.compatibilityProfile ? "compatibility" : "non-compatibility")
        << " compute=" << (caps.computeShaders ? 1 : 0)
        << " ssbo=" << (caps.shaderStorageBuffers ? 1 : 0)
        << " max_compute_invocations=" << caps.maxComputeWorkGroupInvocations
        << " max_compute_shared_bytes=" << caps.maxComputeSharedMemoryBytes
        << " max_ssbo_bindings=" << caps.maxShaderStorageBufferBindings
        << " vendor=\"" << caps.vendor << '"'
        << " renderer=\"" << caps.renderer << '"'
        << " version=\"" << caps.version << '"'
        << " glsl=\"" << caps.shadingLanguageVersion << '"'
        << '\n';

    if (caps.meetsOpenGl43Baseline())
        return;

    std::ostringstream message;
    message
        << "EliteGame requires OpenGL 4.3+ compatibility profile with compute shaders and SSBO support. "
        << "Detected context " << caps.contextMajor << '.' << caps.contextMinor
        << ", GLAD " << caps.gladMajor << '.' << caps.gladMinor
        << ", compatibility=" << (caps.compatibilityProfile ? 1 : 0)
        << ", compute=" << (caps.computeShaders ? 1 : 0)
        << ", ssbo=" << (caps.shaderStorageBuffers ? 1 : 0);
    throw std::runtime_error(message.str());
}

} // namespace render::gpu
