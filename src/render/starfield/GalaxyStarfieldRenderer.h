#pragma once

#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/render/starfield/MilkyWayRenderer.h"

class GalaxyStarfieldRenderer
{
public:

    struct RealStar
    {
        std::string id;
        std::string name;

        glm::vec3 positionLy {0.0f};
        glm::vec3 color {1.0f};

        float visualMagnitudeFromSol = 6.0f;
        float absoluteMagnitude = 6.0f;

        bool isGameSystem = false;
        int gameSystemId = -1;
    };

    GalaxyStarfieldRenderer();
    ~GalaxyStarfieldRenderer();

    GalaxyStarfieldRenderer(const GalaxyStarfieldRenderer&) = delete;
    GalaxyStarfieldRenderer& operator=(const GalaxyStarfieldRenderer&) = delete;

    bool initialize(const std::string& atlasPath = "assets/data/galaxy/star_systems.json");
    void shutdown();

    void setObserverPositionLy(const glm::vec3& positionLy);

    void render(
        const glm::mat4& view,
        const glm::mat4& projection,
        float sizeScale = 1.0f
    );

    bool isInitialized() const { return m_initialized; }
    std::size_t starCount() const { return m_vertices.size(); }

    // --- PUBLIC ACCESS FOR HUD / DEBUG ---

    const std::vector<RealStar>& getRealStars() const
    {
        return m_realStars;
    }

    const glm::vec3& getObserverPositionLy() const
    {
        return m_observerPositionLy;
    }

    float renderRadius() const
    {
        return m_renderRadius;
    }

private:
    struct StarSource
    {

        glm::vec3 positionLy {0.0f};
        glm::vec3 color {1.0f};
        float magnitude = 1.0f;
        float size = 1.0f;
        bool atlasStar = false;
    };

    

    struct StarVertex
    {
        glm::vec3 position {0.0f};
        glm::vec4 color {1.0f};
        float size = 1.0f;
        glm::vec2 uv {0.5f, 0.5f};
    };

    

    bool loadAtlasStars(const std::string& atlasPath);
    bool loadRealStarCatalog(const std::string& path);

    void generateProceduralField();

    void rebuildVertices();
    void rebuildVerticesFromRealCatalog();



    static glm::vec3 colorForStarType(const std::string& type);

private:
    bool m_initialized = false;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    
    GLuint m_screenVao = 0;
    GLuint m_screenVbo = 0;
    std::vector<StarVertex> m_cachedScreenVerts;
    
    glm::mat4 m_lastStarMvp {1.0f};
    

    int m_lastViewportX = -1;
    int m_lastViewportY = -1;
    int m_lastViewportW = -1;
    int m_lastViewportH = -1;

    float m_lastSizeScale = -1.0f;
    bool m_starScreenCacheDirty = true;    

    std::vector<StarSource> m_sources;
    std::vector<RealStar> m_realStars;

    std::vector<StarVertex> m_vertices;
    

    glm::vec3 m_observerPositionLy {0.0f};
    glm::vec3 m_lastRebuildObserverPositionLy {0.0f};
    float m_rebuildThresholdLy = 0.01f;   // можно потом крутить
    float m_renderRadius = 120.0f;

    MilkyWayRenderer m_milkyWayRenderer;
};
