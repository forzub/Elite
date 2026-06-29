#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace render::celestial
{
    struct CelestialShapeVertex
    {
        glm::vec3 pos {0.0f};
        glm::vec2 uv {0.0f};
    };

    struct CelestialShapeTriangle
    {
        CelestialShapeVertex a;
        CelestialShapeVertex b;
        CelestialShapeVertex c;
    };

    struct CelestialShapeMesh
    {
        std::vector<CelestialShapeTriangle> triangles;

        glm::vec3 minBounds {0.0f};
        glm::vec3 maxBounds {0.0f};
        glm::vec3 boundCenter {0.0f};

        float boundRadius = 0.0f;

        GLuint albedoTexture = 0;

        bool valid() const
        {
            return
                !triangles.empty() &&
                boundRadius > 0.000001f;
        }
    };

    class CelestialShapeMeshLibrary
    {
    public:
        const CelestialShapeMesh* load(
            const std::string& objPath,
            const std::string& albedoPath,
            bool flipTexcoordV = true
        );

    private:
        bool loadObjMesh(
            const std::string& objPath,
            CelestialShapeMesh& outMesh,
            bool flipTexcoordV
        );

        GLuint loadAlbedoTexture(
            const std::string& albedoPath
        );

    private:
        std::unordered_map<std::string, CelestialShapeMesh> m_meshByPath;
        std::unordered_map<std::string, bool> m_meshLoadAttemptedByPath;

        std::unordered_map<std::string, GLuint> m_textureByPath;
        std::unordered_map<std::string, bool> m_textureLoadAttemptedByPath;
    };
}