#pragma once

#include <string>

#include <glad/gl.h>
#include <glm/glm.hpp>

class MilkyWayRenderer
{
public:
    MilkyWayRenderer() = default;
    ~MilkyWayRenderer();

    bool initialize(
        const std::string& path =
            "assets/data/galaxy/milky_way.json"
    );

    void shutdown();

    void setObserverPositionLy(
        const glm::vec3& observerLy
    );

    void render(
        const glm::mat4& view,
        const glm::mat4& projection,
        float skyRadius
    );

    bool isInitialized() const
    {
        return m_initialized;
    }

private:
    bool loadConfig(
        const std::string& path
    );

    void ensureHalfResolutionTarget(
        int width,
        int height
    );

private:
    bool m_initialized = false;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    /*
        Half-resolution target только для мягкой
        галактической дымки.

        Сами звёзды продолжают рисоваться отдельно
        в полном разрешении.
    */
    GLuint m_halfFramebuffer = 0;
    GLuint m_halfTexture = 0;

    GLuint m_compositeVao = 0;
    GLuint m_compositeShader = 0;

    GLint m_compositeTextureLocation = -1;

    int m_halfWidth = 0;
    int m_halfHeight = 0;

    bool m_halfResolutionWarningPrinted = false;

    glm::vec3 m_observerLy {
        0.0f
    };

    // Векторы в глобальной системе движка:
    // +X right, +Y up, -Z forward.
    glm::vec3 m_galacticCenterDir {
        -0.0549f,
        -0.8734f,
        -0.4838f
    };

    glm::vec3 m_galacticNorthDir {
        -0.8676f,
        -0.1981f,
        0.4559f
    };

    float m_bandWidthDeg = 18.0f;
    float m_coreWidthDeg = 32.0f;

    float m_intensity = 0.42f;
    float m_dustStrength = 0.72f;
};