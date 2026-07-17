#include "src/render/system_map/HubMapGpuGeometryRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <utility>

#include "src/game/geometry/MeshGPU.h"
#include "src/render/ShaderLibrary.h"

namespace render::system_map
{

namespace
{

void restoreCapability(
    GLenum capability,
    GLboolean wasEnabled
)
{
    if (wasEnabled)
    {
        glEnable(
            capability
        );
    }
    else
    {
        glDisable(
            capability
        );
    }
}

} // namespace


bool HubMapGpuGeometryRenderer::sameColor(
    const glm::vec4& a,
    const glm::vec4& b
)
{
    return
        a.r == b.r &&
        a.g == b.g &&
        a.b == b.b &&
        a.a == b.a;
}


void HubMapGpuGeometryRenderer::ensureResources()
{
    if (m_assemblyShader == 0)
    {
        m_assemblyShader =
            ShaderLibrary::instance().get(
                "hub_map_assembly_wire"
            );
    }

    if (m_dynamicLineShader == 0)
    {
        m_dynamicLineShader =
            ShaderLibrary::instance().get(
                "hub_map_dynamic_lines"
            );
    }

    if (m_assemblyShader == 0 ||
        m_dynamicLineShader == 0)
    {
        if (!m_missingShaderWarningPrinted)
        {
            m_missingShaderWarningPrinted = true;

            std::cerr
                << "[HubMapGpuGeometryRenderer]"
                << " required shaders are unavailable"
                << " assembly=" << m_assemblyShader
                << " dynamic=" << m_dynamicLineShader
                << "\n";
        }

        return;
    }

    if (m_assemblyViewportSizeLocation < 0)
    {
        m_assemblyViewportSizeLocation =
            glGetUniformLocation(
                m_assemblyShader,
                "uViewportSizePx"
            );

        m_assemblyScreenOriginLocation =
            glGetUniformLocation(
                m_assemblyShader,
                "uScreenOriginPx"
            );

        m_assemblyPixelsPerMeterLocation =
            glGetUniformLocation(
                m_assemblyShader,
                "uPixelsPerMeter"
            );

        m_assemblyCameraTrigLocation =
            glGetUniformLocation(
                m_assemblyShader,
                "uCameraTrig"
            );

        m_assemblyColorLocation =
            glGetUniformLocation(
                m_assemblyShader,
                "uColor"
            );

        m_dynamicViewportSizeLocation =
            glGetUniformLocation(
                m_dynamicLineShader,
                "uViewportSizePx"
            );

        m_dynamicScreenOriginLocation =
            glGetUniformLocation(
                m_dynamicLineShader,
                "uScreenOriginPx"
            );

        m_dynamicPixelsPerMeterLocation =
            glGetUniformLocation(
                m_dynamicLineShader,
                "uPixelsPerMeter"
            );

        m_dynamicCameraTrigLocation =
            glGetUniformLocation(
                m_dynamicLineShader,
                "uCameraTrig"
            );
    }

    if (m_dynamicLineVao != 0 &&
        m_dynamicLineVbo != 0)
    {
        return;
    }

    glGenVertexArrays(
        1,
        &m_dynamicLineVao
    );

    glGenBuffers(
        1,
        &m_dynamicLineVbo
    );

    glBindVertexArray(
        m_dynamicLineVao
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_dynamicLineVbo
    );

    glEnableVertexAttribArray(
        0
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(
            DynamicLineVertex
        ),
        reinterpret_cast<void*>(
            offsetof(
                DynamicLineVertex,
                position
            )
        )
    );

    glEnableVertexAttribArray(
        1
    );

    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(
            DynamicLineVertex
        ),
        reinterpret_cast<void*>(
            offsetof(
                DynamicLineVertex,
                color
            )
        )
    );

    glEnableVertexAttribArray(
        2
    );

    glVertexAttribPointer(
        2,
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(
            DynamicLineVertex
        ),
        reinterpret_cast<void*>(
            offsetof(
                DynamicLineVertex,
                coordinateSpace
            )
        )
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        0
    );

    glBindVertexArray(
        0
    );
}


void HubMapGpuGeometryRenderer::beginFrame(
    int viewportWidth,
    int viewportHeight,
    const glm::dvec2& screenOriginPx,
    double pixelsPerMeter,
    double yaw,
    double pitch
)
{
    ensureResources();

    m_meshBatches.clear();
    m_dynamicLineVertices.clear();

    m_viewportSizePx =
        glm::vec2(
            static_cast<float>(
                std::max(
                    viewportWidth,
                    1
                )
            ),
            static_cast<float>(
                std::max(
                    viewportHeight,
                    1
                )
            )
        );

    m_screenOriginPx =
        glm::vec2(
            static_cast<float>(
                screenOriginPx.x
            ),
            static_cast<float>(
                screenOriginPx.y
            )
        );

    m_pixelsPerMeter =
        static_cast<float>(
            pixelsPerMeter
        );

    m_cameraTrig =
        glm::vec4(
            static_cast<float>(
                std::cos(
                    yaw
                )
            ),
            static_cast<float>(
                std::sin(
                    yaw
                )
            ),
            static_cast<float>(
                std::cos(
                    pitch
                )
            ),
            static_cast<float>(
                std::sin(
                    pitch
                )
            )
        );

    m_frameActive =
        m_assemblyShader != 0 &&
        m_dynamicLineShader != 0 &&
        m_dynamicLineVao != 0 &&
        m_dynamicLineVbo != 0;
}


HubMapGpuGeometryRenderer::MeshBatch&
HubMapGpuGeometryRenderer::batchFor(
    const render::MeshGPU& mesh,
    const glm::vec4& color
)
{
    for (auto& batch : m_meshBatches)
    {
        if (batch.mesh == &mesh &&
            sameColor(
                batch.color,
                color
            ))
        {
            return
                batch;
        }
    }

    MeshBatch batch;

    batch.mesh =
        &mesh;

    batch.color =
        color;

    m_meshBatches.push_back(
        std::move(
            batch
        )
    );

    return
        m_meshBatches.back();
}


void HubMapGpuGeometryRenderer::submitMesh(
    const render::MeshGPU& mesh,
    const glm::mat4& localToHub,
    const glm::vec4& color
)
{
    if (!m_frameActive ||
        mesh.getEdgeVertexCount() == 0)
    {
        return;
    }

    batchFor(
        mesh,
        color
    ).models.push_back(
        localToHub
    );
}


void HubMapGpuGeometryRenderer::submitHubLine(
    const glm::dvec3& a,
    const glm::dvec3& b,
    const glm::vec4& color
)
{
    if (!m_frameActive)
        return;

    DynamicLineVertex first;

    first.position =
        glm::vec3(
            a
        );

    first.color =
        color;

    first.coordinateSpace =
        0.0f;

    DynamicLineVertex second;

    second.position =
        glm::vec3(
            b
        );

    second.color =
        color;

    second.coordinateSpace =
        0.0f;

    m_dynamicLineVertices.push_back(
        first
    );

    m_dynamicLineVertices.push_back(
        second
    );
}


void HubMapGpuGeometryRenderer::submitScreenLine(
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::vec4& color
)
{
    if (!m_frameActive)
        return;

    DynamicLineVertex first;

    first.position =
        glm::vec3(
            static_cast<float>(
                a.x
            ),
            static_cast<float>(
                a.y
            ),
            0.0f
        );

    first.color =
        color;

    first.coordinateSpace =
        1.0f;

    DynamicLineVertex second;

    second.position =
        glm::vec3(
            static_cast<float>(
                b.x
            ),
            static_cast<float>(
                b.y
            ),
            0.0f
        );

    second.color =
        color;

    second.coordinateSpace =
        1.0f;

    m_dynamicLineVertices.push_back(
        first
    );

    m_dynamicLineVertices.push_back(
        second
    );
}


void HubMapGpuGeometryRenderer::submitScreenCircle(
    const glm::dvec2& centerPx,
    double radiusPx,
    const glm::vec4& color,
    int segments
)
{
    if (!m_frameActive ||
        radiusPx <= 0.5)
    {
        return;
    }

    segments =
        std::max(
            8,
            segments
        );

    constexpr double twoPi =
        6.28318530717958647692;

    for (int index = 0;
         index < segments;
         ++index)
    {
        const double angle0 =
            twoPi *
            static_cast<double>(
                index
            ) /
            static_cast<double>(
                segments
            );

        const double angle1 =
            twoPi *
            static_cast<double>(
                index + 1
            ) /
            static_cast<double>(
                segments
            );

        submitScreenLine(
            centerPx +
                glm::dvec2(
                    std::cos(
                        angle0
                    ),
                    std::sin(
                        angle0
                    )
                ) *
                radiusPx,

            centerPx +
                glm::dvec2(
                    std::cos(
                        angle1
                    ),
                    std::sin(
                        angle1
                    )
                ) *
                radiusPx,

            color
        );
    }
}


void HubMapGpuGeometryRenderer::submitScreenCross(
    const glm::dvec2& centerPx,
    double halfSizePx,
    const glm::vec4& color
)
{
    if (!m_frameActive ||
        halfSizePx <= 0.0)
    {
        return;
    }

    submitScreenLine(
        glm::dvec2(
            centerPx.x -
                halfSizePx,
            centerPx.y
        ),
        glm::dvec2(
            centerPx.x +
                halfSizePx,
            centerPx.y
        ),
        color
    );

    submitScreenLine(
        glm::dvec2(
            centerPx.x,
            centerPx.y -
                halfSizePx
        ),
        glm::dvec2(
            centerPx.x,
            centerPx.y +
                halfSizePx
        ),
        color
    );
}


void HubMapGpuGeometryRenderer::applyFrameUniforms(
    GLuint shader
) const
{
    const bool assembly =
        shader ==
        m_assemblyShader;

    const GLint viewportSizeLocation =
        assembly
            ? m_assemblyViewportSizeLocation
            : m_dynamicViewportSizeLocation;

    const GLint screenOriginLocation =
        assembly
            ? m_assemblyScreenOriginLocation
            : m_dynamicScreenOriginLocation;

    const GLint pixelsPerMeterLocation =
        assembly
            ? m_assemblyPixelsPerMeterLocation
            : m_dynamicPixelsPerMeterLocation;

    const GLint cameraTrigLocation =
        assembly
            ? m_assemblyCameraTrigLocation
            : m_dynamicCameraTrigLocation;

    if (viewportSizeLocation >= 0)
    {
        glUniform2f(
            viewportSizeLocation,
            m_viewportSizePx.x,
            m_viewportSizePx.y
        );
    }

    if (screenOriginLocation >= 0)
    {
        glUniform2f(
            screenOriginLocation,
            m_screenOriginPx.x,
            m_screenOriginPx.y
        );
    }

    if (pixelsPerMeterLocation >= 0)
    {
        glUniform1f(
            pixelsPerMeterLocation,
            m_pixelsPerMeter
        );
    }

    if (cameraTrigLocation >= 0)
    {
        glUniform4f(
            cameraTrigLocation,
            m_cameraTrig.x,
            m_cameraTrig.y,
            m_cameraTrig.z,
            m_cameraTrig.w
        );
    }
}


void HubMapGpuGeometryRenderer::flush()
{
    if (!m_frameActive)
    {
        m_meshBatches.clear();
        m_dynamicLineVertices.clear();
        return;
    }

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;

    GLint previousBlendSourceRgb = GL_SRC_ALPHA;
    GLint previousBlendDestinationRgb = GL_ONE_MINUS_SRC_ALPHA;
    GLint previousBlendSourceAlpha = GL_SRC_ALPHA;
    GLint previousBlendDestinationAlpha = GL_ONE_MINUS_SRC_ALPHA;

    GLint previousBlendEquationRgb = GL_FUNC_ADD;
    GLint previousBlendEquationAlpha = GL_FUNC_ADD;

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    glGetIntegerv(
        GL_ARRAY_BUFFER_BINDING,
        &previousArrayBuffer
    );

    glGetIntegerv(
        GL_BLEND_SRC_RGB,
        &previousBlendSourceRgb
    );

    glGetIntegerv(
        GL_BLEND_DST_RGB,
        &previousBlendDestinationRgb
    );

    glGetIntegerv(
        GL_BLEND_SRC_ALPHA,
        &previousBlendSourceAlpha
    );

    glGetIntegerv(
        GL_BLEND_DST_ALPHA,
        &previousBlendDestinationAlpha
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &previousBlendEquationAlpha
    );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    glDisable(
        GL_DEPTH_TEST
    );

    glDisable(
        GL_CULL_FACE
    );

    glEnable(
        GL_BLEND
    );

    glBlendEquation(
        GL_FUNC_ADD
    );

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    if (!m_meshBatches.empty())
    {
        glUseProgram(
            m_assemblyShader
        );

        applyFrameUniforms(
            m_assemblyShader
        );

        for (const auto& batch : m_meshBatches)
        {
            if (batch.mesh == nullptr ||
                batch.models.empty())
            {
                continue;
            }

            if (m_assemblyColorLocation >= 0)
            {
                glUniform4f(
                    m_assemblyColorLocation,
                    batch.color.r,
                    batch.color.g,
                    batch.color.b,
                    batch.color.a
                );
            }

            batch.mesh->drawEdgesInstanced(
                batch.models
            );
        }
    }

    if (!m_dynamicLineVertices.empty())
    {
        glUseProgram(
            m_dynamicLineShader
        );

        applyFrameUniforms(
            m_dynamicLineShader
        );

        glBindVertexArray(
            m_dynamicLineVao
        );

        glBindBuffer(
            GL_ARRAY_BUFFER,
            m_dynamicLineVbo
        );

        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                m_dynamicLineVertices.size() *
                sizeof(
                    DynamicLineVertex
                )
            ),
            m_dynamicLineVertices.data(),
            GL_STREAM_DRAW
        );

        glDrawArrays(
            GL_LINES,
            0,
            static_cast<GLsizei>(
                m_dynamicLineVertices.size()
            )
        );
    }

    glBindVertexArray(
        static_cast<GLuint>(
            previousVao
        )
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        static_cast<GLuint>(
            previousArrayBuffer
        )
    );

    glUseProgram(
        static_cast<GLuint>(
            previousProgram
        )
    );

    glBlendEquationSeparate(
        static_cast<GLenum>(
            previousBlendEquationRgb
        ),
        static_cast<GLenum>(
            previousBlendEquationAlpha
        )
    );

    glBlendFuncSeparate(
        static_cast<GLenum>(
            previousBlendSourceRgb
        ),
        static_cast<GLenum>(
            previousBlendDestinationRgb
        ),
        static_cast<GLenum>(
            previousBlendSourceAlpha
        ),
        static_cast<GLenum>(
            previousBlendDestinationAlpha
        )
    );

    restoreCapability(
        GL_BLEND,
        blendWasEnabled
    );

    restoreCapability(
        GL_DEPTH_TEST,
        depthWasEnabled
    );

    restoreCapability(
        GL_CULL_FACE,
        cullWasEnabled
    );

    m_meshBatches.clear();
    m_dynamicLineVertices.clear();
    m_frameActive = false;
}

} // namespace render::system_map
