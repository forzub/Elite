#pragma once

#include <glad/gl.h>

#include <vector>
#include <cstddef>
#include <string>

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

    void renderSphereOnly(
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::vec3& center,
        float radiusScale,
        const glm::vec3& baseColor,
        const glm::vec3& litColor,
        float emission
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


    struct PlanetSolidVertex
    {
        glm::vec3 position;
        glm::vec3 normal;
    };

private:
    bool m_initialized = false;

    GLuint m_shader = 0;
    GLint  m_mvpLocation = -1;

    GLuint m_solidShader = 0;

    GLint m_solidModelLocation = -1;
    GLint m_solidViewLocation = -1;
    GLint m_solidProjLocation = -1;
    GLint m_solidCameraPosLocation = -1;
    GLint m_solidLightDirLocation = -1;

    GLint m_solidBaseColorLocation = -1;
    GLint m_solidLitColorLocation = -1;
    GLint m_solidEmissionLocation = -1;



    GLuint m_staticVao = 0;
    GLuint m_staticVbo = 0;
    GLsizei m_staticVertexCount = 0;

    GLuint m_cloudVao = 0;
    GLuint m_cloudVbo = 0;
    GLsizei m_cloudVertexCount = 0;

    GLuint m_solidVao = 0;
    GLuint m_solidVbo = 0;
    GLuint m_solidIbo = 0;
    GLsizei m_solidIndexCount = 0;

private:
    void buildMeshes();
    void buildSolidSphere();
    void drawSolidSphere(
        const glm::mat4& model,
        const glm::mat4& view,
        const glm::mat4& proj
    );

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

    void buildEarthCoastlinesFromJson(
        std::vector<PlanetLineVertex>& out,
        float radius,
        const std::string& filePath
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

    void addGeoLine(
        std::vector<PlanetLineVertex>& out,
        float radius,
        float latA,
        float lonA,
        float latB,
        float lonB,
        const glm::vec4& color,
        int segments
    ) ;

    void buildEarthCoastlinesFromData(
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