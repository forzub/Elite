#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "render/types/Viewport.h"
#include "src/game/navigation/NavigationRegionCatalog.h"

class Font;

namespace render::navigation
{

struct NavigationCellLabelRequest
{
    game::navigation::NavigationCellId id;
    std::string visualKey;

    glm::vec3 center {0.0f};
    glm::vec3 halfAxisX {0.0f};
    glm::vec3 halfAxisY {0.0f};
    glm::vec3 halfAxisZ {0.0f};

    std::string coordinateText;
    std::string regionName;
    std::string shortRegionName;

    glm::vec4 color {0.75f, 0.82f, 0.92f, 0.62f};
    float targetOpacity = 1.0f;
};

class NavigationCellLabelLayer
{
public:
    NavigationCellLabelLayer();
    ~NavigationCellLabelLayer();

    void clear();

    void draw(
        const Viewport& viewport,
        const glm::mat4& mvp,
        const glm::vec3& cameraPosition,
        const std::vector<NavigationCellLabelRequest>& requests,
        double nowSeconds
    );

private:
    struct Face;

    struct LabelState
    {
        NavigationCellLabelRequest request;
        float opacity = 0.0f;
        float targetOpacity = 0.0f;
        int currentFace = -1;
        int previousFace = -1;
        float faceBlend = 1.0f;
        bool seen = false;
    };

    void ensureFont();

    static std::string stableKey(
        const game::navigation::NavigationCellId& id
    );

    static std::vector<Face> facesFor(
        const NavigationCellLabelRequest& request
    );

    void updateFace(
        LabelState& state,
        const std::vector<Face>& faces,
        const glm::mat4& mvp,
        const glm::vec3& cameraPosition,
        const Viewport& viewport,
        float dt
    );

    void drawFaceLabel(
        const LabelState& state,
        const Face& face,
        const glm::mat4& mvp,
        const Viewport& viewport,
        const glm::vec3& cameraPosition,
        float alpha
    );

private:
    std::unique_ptr<Font> m_font;
    std::unordered_map<std::string, LabelState> m_states;
    double m_lastTimeSeconds = 0.0;
};

} // namespace render::navigation
