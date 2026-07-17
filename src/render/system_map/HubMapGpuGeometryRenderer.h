#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <vector>

namespace render
{
class MeshGPU;
}

namespace render::system_map
{

class HubMapGpuGeometryRenderer
{
public:
    void beginFrame(
        int viewportWidth,
        int viewportHeight,
        const glm::dvec2& screenOriginPx,
        double pixelsPerMeter,
        double yaw,
        double pitch
    );

    bool active() const
    {
        return m_frameActive;
    }

    void submitMesh(
        const render::MeshGPU& mesh,
        const glm::mat4& localToHub,
        const glm::vec4& color
    );

    void submitHubLine(
        const glm::dvec3& a,
        const glm::dvec3& b,
        const glm::vec4& color
    );

    void submitScreenLine(
        const glm::dvec2& a,
        const glm::dvec2& b,
        const glm::vec4& color
    );

    void submitScreenCircle(
        const glm::dvec2& centerPx,
        double radiusPx,
        const glm::vec4& color,
        int segments
    );

    void submitScreenCross(
        const glm::dvec2& centerPx,
        double halfSizePx,
        const glm::vec4& color
    );

    void flush();

private:
    struct MeshBatch
    {
        const render::MeshGPU* mesh = nullptr;

        glm::vec4 color {
            1.0f
        };

        std::vector<
            glm::mat4
        > models;
    };

    struct DynamicLineVertex
    {
        glm::vec3 position {
            0.0f
        };

        glm::vec4 color {
            1.0f
        };

        /*
            0.0 = hub-local meters;
            1.0 = screen pixels.
        */
        float coordinateSpace = 0.0f;
    };

private:
    void ensureResources();

    MeshBatch& batchFor(
        const render::MeshGPU& mesh,
        const glm::vec4& color
    );

    void applyFrameUniforms(
        GLuint shader
    ) const;

    static bool sameColor(
        const glm::vec4& a,
        const glm::vec4& b
    );

private:
    GLuint m_assemblyShader = 0;
    GLuint m_dynamicLineShader = 0;

    GLuint m_dynamicLineVao = 0;
    GLuint m_dynamicLineVbo = 0;

    GLint m_assemblyViewportSizeLocation = -1;
    GLint m_assemblyScreenOriginLocation = -1;
    GLint m_assemblyPixelsPerMeterLocation = -1;
    GLint m_assemblyCameraTrigLocation = -1;
    GLint m_assemblyColorLocation = -1;

    GLint m_dynamicViewportSizeLocation = -1;
    GLint m_dynamicScreenOriginLocation = -1;
    GLint m_dynamicPixelsPerMeterLocation = -1;
    GLint m_dynamicCameraTrigLocation = -1;

    glm::vec2 m_viewportSizePx {
        1.0f,
        1.0f
    };

    glm::vec2 m_screenOriginPx {
        0.0f,
        0.0f
    };

    float m_pixelsPerMeter = 1.0f;

    /*
        x = cos(yaw)
        y = sin(yaw)
        z = cos(pitch)
        w = sin(pitch)
    */
    glm::vec4 m_cameraTrig {
        1.0f,
        0.0f,
        1.0f,
        0.0f
    };

    std::vector<
        MeshBatch
    > m_meshBatches;

    std::vector<
        DynamicLineVertex
    > m_dynamicLineVertices;

    bool m_frameActive = false;
    bool m_missingShaderWarningPrinted = false;
};

} // namespace render::system_map
