#pragma once

#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "src/render/types/Viewport.h"

namespace world::celestial
{
    enum class BodyType;
    struct SystemMapBody;
    struct SystemMapSnapshot;
}

namespace game::system_map
{
    class SystemMapFrameData;

    struct SystemBodyVisualMetrics
    {
        float physicalRadiusWorld = 0.0f;
        float physicalRadiusPx = 0.0f;

        float markerRadiusPx = 0.0f;
        float markerRadiusWorld = 0.0f;

        float pickRadiusPx = 0.0f;

        bool drawPhysicalBody = false;
        bool drawMarker = false;
    };

    /*
        Shared rendering backend used by SystemMapSceneRenderer.

        The scene renderer owns System-map orchestration. The legacy facade
        remains the single owner of OpenGL buffers, shaders, generated
        textures, text services and the shared map background renderers.
        This interface contains rendering operations only; input is handled
        separately by SystemMapInteraction.
    */
    class SystemMapRenderContext
    {
    public:
        virtual ~SystemMapRenderContext() = default;

        virtual void ensureSystemRenderResources() = 0;
        virtual SystemMapFrameData& systemFrameData() = 0;

        virtual double currentTimeSeconds() const = 0;

        virtual void drawMapStarfield(
            const Viewport& viewport,
            const glm::dvec3& observerPositionLy,
            const glm::mat4& cameraView,
            float fieldOfViewDeg,
            float sizeScale,
            bool distantGalaxyBackdrop,
            float starBrightnessScale,
            float milkyWayIntensityScale,
            const glm::vec3& milkyWayColorTint
        ) = 0;

        virtual void drawMapAtmosphereVeil(
            float centerAlpha,
            float edgeAlpha,
            float aquaStrength
        ) = 0;

        virtual void beginLines() = 0;
        virtual void beginSolids() = 0;
        virtual void beginTexturedBodies() = 0;

        virtual void addLine(
            const glm::vec3& a,
            const glm::vec3& b,
            const glm::vec4& color
        ) = 0;

        virtual void addCircleXZ(
            const glm::vec3& center,
            float radius,
            const glm::vec4& color,
            int segments
        ) = 0;

        virtual void addCircleXY(
            const glm::vec3& center,
            float radius,
            const glm::vec4& color,
            int segments
        ) = 0;

        virtual void addCross(
            const glm::vec3& center,
            float size,
            const glm::vec4& color
        ) = 0;

        virtual void addBillboardHalo(
            const glm::vec3& center,
            float bodyRadius,
            float outerRadiusScale,
            float baseAlpha,
            const glm::vec4& color,
            const glm::mat4& view,
            int ringCount,
            int segments
        ) = 0;

        virtual void flushLines(const glm::mat4& mvp) = 0;
        virtual void flushSolids(const glm::mat4& mvp) = 0;
        virtual void flushTexturedBodies(const glm::mat4& mvp) = 0;

        virtual glm::vec4 colorForBodyType(
            world::celestial::BodyType type
        ) const = 0;

        virtual float bodyVisualRadius(
            const world::celestial::SystemMapBody& body,
            float distanceScale
        ) const = 0;

        virtual SystemBodyVisualMetrics computeSystemBodyVisualMetrics(
            const world::celestial::SystemMapBody& body,
            float physicalRadiusWorld,
            double worldUnitsPerPixel
        ) const = 0;

        virtual void addSystemBodyRingVisuals(
            const world::celestial::SystemMapBody& body,
            const glm::vec3& center,
            const SystemBodyVisualMetrics& metrics,
            float systemScale,
            double worldUnitsPerPixel,
            const glm::mat4& view
        ) = 0;

        virtual void addSystemBodyVisual(
            const world::celestial::SystemMapBody& body,
            const glm::vec3& center,
            const SystemBodyVisualMetrics& metrics,
            const glm::vec4& fallbackColor,
            const glm::mat4& view
        ) = 0;

        virtual glm::vec2 projectToScreen(
            const glm::vec3& world,
            const glm::mat4& mvp,
            const Viewport& viewport,
            bool& visible,
            float& depth
        ) const = 0;

        virtual void drawSystemNavigationGrid(
            const Viewport& viewport,
            const glm::mat4& mvp,
            float systemScale
        ) = 0;

        virtual void drawSystemObjectOverlays(
            const world::celestial::SystemMapSnapshot& system,
            const glm::mat4& view,
            const glm::mat4& mvp,
            const std::unordered_map<std::string, glm::vec3>& objectVisualPosById,
            const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
            const std::unordered_map<std::string, float>& drawRadiusById,
            double worldUnitsPerPixel,
            float systemScale
        ) = 0;

        virtual void drawSystemLabels(
            const Viewport& viewport,
            const world::celestial::SystemMapSnapshot& system,
            const glm::mat4& mvp,
            const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
            const std::unordered_map<std::string, float>& drawRadiusById
        ) = 0;

        virtual void drawSystemObjectLabels(
            const Viewport& viewport,
            const world::celestial::SystemMapSnapshot& system,
            const glm::mat4& mvp,
            const glm::mat4& view,
            const std::unordered_map<std::string, glm::vec3>& objectVisualPosById,
            const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
            const std::unordered_map<std::string, float>& drawRadiusById
        ) = 0;

        virtual void beginTextFrame(
            int viewportWidth,
            int viewportHeight
        ) = 0;

        virtual void drawTextPx(
            const std::string& text,
            float x,
            float y,
            int pixelHeight,
            const glm::vec4& color
        ) = 0;

        virtual void endTextFrame() = 0;
    };
}
