#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace debug
{

struct DebugSeamProxy
{
    glm::vec3 centerWorld {0.0f};
    glm::vec3 halfSize {0.1f};
    glm::mat3 orientationWorld {1.0f};

    float score = 0.0f;
    std::string neighborModuleId;
};

struct DebugRenderSettings
{
    // =========================
    // FLAGS
    // =========================
    bool drawMeshes = true;
    bool renderCockpit = true;
    bool renderShipUi = true;
    bool renderStarfield = true;
    bool renderRearCamera = true;
    bool showStarLabels = true;
    bool showAllStarLabels = false;

    // Scene mode selected from debug UI.
    // "game"   = normal update, no scripted promo wing.
    // "promo1" = first scripted flyby scene.
    std::string sceneMode = "game";

    // Debug-only sky overlay: when enabled, hovering the mouse near
    // a known constellation draws its stick lines and label.
    bool showConstellationHover = false;
    float constellationHoverRadiusPx = 140.0f;

    bool drawAxes = false;          // legacy alias
    bool drawWorldAxes = false;
    bool drawObjectAxes = false;
    bool drawModulePivots = false;
    bool drawHitVolumes = false;
    bool drawSeamDebug = true;

    // 0 = all, 1 = primary only, 2 = support-link only
    int hitVolumeFilterMode = 0;

    bool publishObjectOrientation = false;

    bool hitVolumesOverlay = true;
    bool hideMeshesWhenDrawingHitVolumes = false;

    // =========================
    // SIZES
    // =========================
    float worldAxisLength  = 120.0f;
    float shipAxisLength   = 18.0f;
    float objectAxisLength = 30.0f;

    float moduleAxisLength = 30.0f;
    float moduleCrossSize  = 2.0f;
    float rotAxisLength    = 30.0f;

    // =========================
    // COLORS
    // =========================
    glm::vec4 axisXColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 axisYColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    glm::vec4 axisZColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

    glm::vec4 moduleOriginColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 modulePivotColor  = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
    glm::vec4 rotationAxisColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);

    // =========================
    // OVERLAY MODES
    // =========================
    bool axesOverlay = true;
    bool crossesOverlay = true;
    bool linesOverlay = true;

    bool captureSeamDebug = true;
    std::vector<DebugSeamProxy> seamDebugProxies;

    bool shouldRenderCockpit() const
    {
        return renderCockpit && renderShipUi;
    }

    bool shouldRenderShipUi() const
    {
        return renderShipUi;
    }

    bool shouldRenderRearCamera() const
    {
        return renderShipUi && renderRearCamera;
    }

    bool shouldRenderStarfield() const
    {
        return renderStarfield;
    }

    bool shouldDrawMeshes() const
    {
        if (!drawMeshes)
            return false;

        if (drawHitVolumes && hideMeshesWhenDrawingHitVolumes)
            return false;

        return true;
    }

    bool shouldDrawWorldAxes() const
    {
        return drawAxes || drawWorldAxes;
    }

    bool shouldDrawObjectAxes() const
    {
        return drawAxes || drawObjectAxes;
    }
};

struct DebugSettings
{
    DebugRenderSettings render;
};

DebugSettings& get();

} // namespace debug