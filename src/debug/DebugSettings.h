#pragma once

#include <glm/glm.hpp>

namespace debug
{

struct DebugRenderSettings
{
    // =========================
    // FLAGS
    // =========================
    bool drawMeshes = true;

    bool drawAxes = false;          // legacy alias
    bool drawWorldAxes = false;
    bool drawObjectAxes = false;
    bool drawModulePivots = false;
    bool drawHitVolumes = false;

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