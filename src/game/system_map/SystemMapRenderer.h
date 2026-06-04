#pragma once

#include <glad/gl.h>

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

#include "render/types/Viewport.h"

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

class SystemMapRenderer
{
public:
    enum class Mode
    {
        Galaxy,
        System
    };

public:
    void init();
    void setRightPanelRatio(float ratio);
    void render(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
    );

    void resetView();

    void focusGalaxySystem(
        int systemId,
        const world::celestial::GalaxyMapSnapshot& galaxy
    );

    int selectedSystemId() const;

void handleInput(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy
);

    void setMode(Mode mode);
    Mode mode() const;
    int focusedSystemId() const;

private:
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec4 color;
    };

    struct ScreenPoint
    {
        int systemId = -1;
        std::string name;
        glm::vec3 world {0.0f};
        glm::vec2 screen {0.0f};
        float depth = 0.0f;
        bool visible = false;
    };

    struct GalaxyCamera
    {
        float yaw = 0.65f;
        float pitch = 0.72f;
        float distance = 150.0f;
        glm::vec3 target {0.0f, 0.0f, 0.0f};

        bool rotating = false;
        bool panning = false;
        bool leftWasDown = false;
        bool rightWasDown = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;

        float lastWheelY = 0.0f;
    };

    struct SystemCamera
    {
        float yaw = 0.45f;
        float pitch = 0.85f;
        float distance = 95.0f;
        glm::vec3 target {0.0f, 0.0f, 0.0f};

        bool rotating = false;
        bool panning = false;
    };

    

private:
    void ensureGlObjects();
    void ensureShader();

    void ensureBackground();
    void drawBackground();  

    void beginLines();
    void addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
    void addCircleXZ(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    );

    void addCircleXY(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    );

    void addCircleYZ(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    );

    void addSphereWire(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color
    );





    void addCross(
        const glm::vec3& center,
        float size,
        const glm::vec4& color
    );
    void flushLines(const glm::mat4& mvp);

    void renderGalaxy(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& nav
    );

    void renderSystem(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
    );

    void drawPanelText(
        const Viewport& vp,
        const std::string& title,
        const std::vector<std::string>& lines
    );

    glm::vec4 colorForStarType(const std::string& starType) const;
    glm::vec4 colorForBodyType(world::celestial::BodyType type) const;

    float bodyVisualRadius(
        const world::celestial::CelestialBodyState& body,
        float distanceScale
    ) const;
        glm::mat4 galaxyViewMatrix() const;
    glm::mat4 galaxyProjectionMatrix(const Viewport& vp) const;

    glm::mat4 systemViewMatrix() const;
    glm::mat4 systemProjectionMatrix(const Viewport& vp) const;

    glm::vec3 galaxyStarPosition(
        const world::celestial::GalaxyMapSystem& s
    ) const;

    glm::vec2 projectToScreen(
        const glm::vec3& world,
        const glm::mat4& mvp,
        const Viewport& vp,
        bool& visible,
        float& depth
    ) const;

    int pickGalaxySystem(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        double mouseX,
        double mouseY
    ) const;

    void drawGalaxyLabels(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const glm::mat4& mvp
    );



    void drawNavigationLayerPlaceholder();

    glm::vec3 nearestVisibleStarToViewportCenter(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        bool& found
    ) const;

    void debugLabelTraceToFile(
        const std::string& name,
        const glm::vec2& screen,
        const glm::vec2& pos
    ) const;

    

private:
    bool m_initialized = false;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    GLuint m_shader = 0;
    GLint  m_mvpLoc = -1;

    GLuint m_bgVao = 0;
    GLuint m_bgVbo = 0;
    GLuint m_bgShader = 0;

    std::vector<Vertex> m_vertices;

    Mode m_mode = Mode::Galaxy;
    float m_rightPanelRatio = 0.28f;

    GalaxyCamera m_galaxyCamera;
    SystemCamera m_systemCamera;

    int m_selectedSystemId = -1;
    int m_focusedSystemId = -1;
    bool m_comboOpen = false;

    std::vector<ScreenPoint> m_lastGalaxyScreenPoints;
};