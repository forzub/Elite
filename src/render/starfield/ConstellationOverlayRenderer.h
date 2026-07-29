#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

class ConstellationOverlayRenderer
{
public:
    struct StarReference
    {
        int brightStarCatalogId = -1;
        glm::vec3 positionLy {0.0f};
    };

    struct ConstellationDefinition
    {
        std::string id;
        std::string name;
        std::vector<std::vector<int>> polylinesHr;
    };

    ConstellationOverlayRenderer() = default;
    ~ConstellationOverlayRenderer();

    ConstellationOverlayRenderer(
        const ConstellationOverlayRenderer&
    ) = delete;

    ConstellationOverlayRenderer& operator=(
        const ConstellationOverlayRenderer&
    ) = delete;

    bool initialize(const std::string& path);
    void shutdown();

    void rebuild(
        const std::vector<StarReference>& stars,
        const glm::vec3& observerPositionLy,
        float skyRadius
    );

    void render(const glm::mat4& mvp) const;

    bool isInitialized() const
    {
        return m_initialized;
    }

    std::size_t segmentCount() const
    {
        return m_vertices.size() / 2;
    }

    const std::vector<ConstellationDefinition>& definitions() const
    {
        return m_definitions;
    }

private:
    struct LineVertex
    {
        glm::vec3 position {0.0f};
        glm::vec4 color {1.0f};
    };

    bool loadDefinitions(const std::string& path);
    void uploadVertices();

private:
    bool m_initialized = false;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    std::vector<ConstellationDefinition> m_definitions;
    std::vector<LineVertex> m_vertices;
};
