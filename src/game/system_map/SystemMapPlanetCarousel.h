#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/render/types/Viewport.h"
#include "src/world/celestial/SystemMapTypes.h"

class Font;

namespace render::hud
{
class HudPrimitiveBatch;
}

namespace game::system_map
{
/*
    System map planet carousel (author request 2026-11-12): a strip at the bottom of the System map with the
    system's planets as textured globes, sized in proportion to each other, named underneath, in order of
    distance from their star (stars in snapshot order, circumbinary planets after them). The strip scrolls
    with the mouse wheel, by dragging, or with the triangle buttons at its ends; when every planet fits there
    is no scrolling and no buttons. A click on a planet selects it on the System map (the owner applies the
    selection, the DETAILS button then opens it).

    The globes use the map's own textures (albedo + the CloudWeatherField clouds, the same the Detail map
    draws), so showing the strip also loads every planet's surface and clouds before the player opens one -
    the Detail map of a planet no longer waits ~1 s for them.
*/
class SystemMapPlanetCarousel
{
public:
    // albedo / clouds of a body for its globe (0 = not ready: a plain disc is drawn meanwhile)
    using TextureSource = std::function<void(const world::celestial::SystemMapBody&, GLuint& albedo, GLuint& clouds)>;

    SystemMapPlanetCarousel();
    ~SystemMapPlanetCarousel();
    SystemMapPlanetCarousel(const SystemMapPlanetCarousel&) = delete;
    SystemMapPlanetCarousel& operator=(const SystemMapPlanetCarousel&) = delete;

    // GL thread, once per System map frame after the scene: lays the strip out and draws it.
    void draw(const Viewport& viewport, const world::celestial::SystemMapSnapshot& system,
              const std::string& selectedBodyId, const TextureSource& textures);

    // Pointer over the strip (local viewport px, layout of the last draw). Returns true when the strip
    // takes the pointer (the map below must not see it); wheelY is consumed; clickedBodyId = a planet that
    // was clicked (press and release on it without dragging).
    bool handlePointer(double x, double y, bool leftDown, double& wheelY, std::string& clickedBodyId);

    // DEV pointer: screen centre of a planet's globe when it is shown in the strip (else x < 0)
    glm::vec2 itemScreenCenter(const std::string& bodyId) const;

    void reset();
    bool visible() const noexcept { return m_visible; }

    // the strip's screen rectangle in the last draw (left, top, right, bottom); empty when hidden
    glm::vec4 bounds() const noexcept { return m_visible ? m_strip : glm::vec4(0.0f); }

private:
    struct Item
    {
        std::string bodyId;
        std::string name;
        float diameterPx = 0.0f;
        float slotWidthPx = 0.0f;
        float centerX = 0.0f; // content coordinates (before scrolling)
        float rotationPhase = 0.0f;
        float axialTiltRad = 0.0f;
        const world::celestial::SystemMapBody* body = nullptr;
    };

    void layout(const Viewport& viewport, const world::celestial::SystemMapSnapshot& system);
    void ensureFont();
    void ensureGlobeProgram();
    void drawGlobe(const Viewport& viewport, float cx, float cy, float diameter, GLuint albedo, GLuint clouds,
                   float rotationPhase, float tilt, bool selected, bool hovered);
    int itemAt(double x, double y) const;
    float maxScroll() const;

    std::unique_ptr<Font> m_font;
    std::unique_ptr<render::hud::HudPrimitiveBatch> m_batch;
    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    int m_systemId = -1;
    std::vector<Item> m_items;
    bool m_visible = false;
    bool m_scrollable = false;
    glm::vec4 m_strip {0.0f};       // left, top, right, bottom
    glm::vec4 m_content {0.0f};     // the globes' area inside the arrows
    glm::vec4 m_leftArrow {0.0f};
    glm::vec4 m_rightArrow {0.0f};
    float m_contentWidth = 0.0f;
    float m_scroll = 0.0f;
    float m_globeRowY = 0.0f;
    float m_labelBaselineY = 0.0f;
    float m_screenScale = 1.0f;

    int m_hovered = -1;
    int m_hoveredArrow = 0;         // -1 left, +1 right
    bool m_leftWasDown = false;
    bool m_pressInside = false;
    double m_pressX = 0.0;
    float m_pressScroll = 0.0f;
    bool m_dragging = false;
    int m_pressItem = -1;
};
} // namespace game::system_map
