#pragma once

#include <cstddef>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/render/starfield/SkyCultureCatalog.h"

class ConstellationOverlayRenderer
{
public:
    struct StarReference
    {
        int brightStarCatalogId = -1; // HR
        int hipparcosCatalogId = -1;  // HIP
        glm::vec3 positionLy {0.0f};

        int catalogId(SkyCultureCatalog::StarIdentifier identifier) const;
        bool skyDirection(
            const glm::vec3& observerPositionLy,
            glm::vec3& outDirection
        ) const;
    };

    using ConstellationDefinition = SkyCultureCatalog::Constellation;

    struct LabelAnchor
    {
        std::size_t definitionIndex = 0;
        glm::vec3 skyPosition {0.0f};
    };

    ConstellationOverlayRenderer() = default;
    ~ConstellationOverlayRenderer();

    ConstellationOverlayRenderer(const ConstellationOverlayRenderer&) = delete;
    ConstellationOverlayRenderer& operator=(const ConstellationOverlayRenderer&) = delete;

    bool initialize();
    void shutdown();

    void setCulture(const SkyCultureCatalog::Culture& culture);

    void rebuild(
        const std::vector<StarReference>& stars,
        const glm::vec3& observerPositionLy,
        float skyRadius
    );

    void render(const glm::mat4& mvp) const;

    bool isInitialized() const { return m_initialized; }
    std::size_t segmentCount() const { return m_vertices.size() / 2; }

    const std::vector<ConstellationDefinition>& definitions() const
    {
        return m_definitions;
    }

    const std::vector<LabelAnchor>& labelAnchors() const
    {
        return m_labelAnchors;
    }

    SkyCultureCatalog::StarIdentifier starIdentifier() const
    {
        return m_starIdentifier;
    }

private:
    struct LineVertex
    {
        glm::vec3 position {0.0f};
        glm::vec4 color {1.0f};
    };

    void uploadVertices();

private:
    bool m_initialized = false;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    SkyCultureCatalog::StarIdentifier m_starIdentifier =
        SkyCultureCatalog::StarIdentifier::BrightStarHr;
    std::vector<ConstellationDefinition> m_definitions;
    std::vector<LabelAnchor> m_labelAnchors;
    std::vector<LineVertex> m_vertices;
};
