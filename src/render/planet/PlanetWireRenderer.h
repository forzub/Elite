#pragma once

#include <glad/gl.h>

#include <vector>
#include <cstddef>

#include <glm/glm.hpp>

class PlanetWireRenderer
{
public:
    bool initialize();

    void render(
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::vec3& planetCenter,
        float timeSeconds
    );

    bool isInitialized() const
    {
        return m_initialized;
    }

private:
    struct PlanetLineVertex
    {
        glm::vec3 position;
        glm::vec4 color;
    };

private:
    bool m_initialized = false;

    GLuint m_shader = 0;
    GLint  m_mvpLocation = -1;

    GLuint m_staticVao = 0;
    GLuint m_staticVbo = 0;
    GLsizei m_staticVertexCount = 0;

    GLuint m_cloudVao = 0;
    GLuint m_cloudVbo = 0;
    GLsizei m_cloudVertexCount = 0;

private:
    void buildMeshes();

    void uploadBuffer(
        GLuint& vao,
        GLuint& vbo,
        const std::vector<PlanetLineVertex>& vertices,
        GLsizei& outVertexCount
    );

    void drawBuffer(
        GLuint vao,
        GLsizei vertexCount,
        const glm::mat4& mvp,
        float lineWidth
    );

    void addLine(
        std::vector<PlanetLineVertex>& out,
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec4& color
    );

    void addLatitudeLine(
        std::vector<PlanetLineVertex>& out,
        float radius,
        float latitudeDeg,
        int segments,
        const glm::vec4& color
    );

    void addLongitudeLine(
        std::vector<PlanetLineVertex>& out,
        float radius,
        float longitudeDeg,
        int segments,
        const glm::vec4& color
    );

    void addGreatCircleXz(
        std::vector<PlanetLineVertex>& out,
        float radius,
        int segments,
        const glm::vec4& color
    );

    void addGreatCircleXy(
        std::vector<PlanetLineVertex>& out,
        float radius,
        int segments,
        const glm::vec4& color
    );

    void addGreatCircleYz(
        std::vector<PlanetLineVertex>& out,
        float radius,
        int segments,
        const glm::vec4& color
    );

    void buildProceduralContinents(
        std::vector<PlanetLineVertex>& out,
        float radius
    );

    void buildEarthTopography(
        std::vector<PlanetLineVertex>& out,
        float radius
    );

    void buildEarthMountainRanges(
        std::vector<PlanetLineVertex>& out,
        float radius
    );

    void buildCloudLayer(
        std::vector<PlanetLineVertex>& out,
        float radius
    );

    glm::vec3 spherePoint(
        float radius,
        float latitudeDeg,
        float longitudeDeg
    ) const;

    float planetNoise(const glm::vec3& p) const;

    float earthField(
        float latitudeDeg,
        float longitudeDeg
    ) const;

    float earthHeight(
        float latitudeDeg,
        float longitudeDeg
    ) const;

    bool isEarthLand(
        float latitudeDeg,
        float longitudeDeg
    ) const;
};