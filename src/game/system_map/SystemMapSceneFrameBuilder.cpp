#include "src/game/system_map/SystemMapSceneFrameBuilder.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/system_map/SystemMapPresentation.h"
#include "src/game/system_map/SystemMapView.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::system_map
{
namespace
{

    double calculatePerspectiveWorldUnitsPerPixel(
        const glm::vec3& worldPosition,
        const glm::mat4& view,
        float fieldOfViewDeg,
        int viewportHeight,
        double fallback
    )
    {
        const glm::vec4 cameraSpace =
            view * glm::vec4(worldPosition, 1.0f);

        const double depth =
            -static_cast<double>(cameraSpace.z);

        if (!std::isfinite(depth) ||
            depth <= 0.000000001)
        {
            return fallback;
        }

        const double halfFieldOfView =
            glm::radians(
                static_cast<double>(fieldOfViewDeg) *
                0.5
            );

        const double safeViewportHeight =
            static_cast<double>(
                std::max(viewportHeight, 1)
            );

        const double result =
            2.0 *
            depth *
            std::tan(halfFieldOfView) /
            safeViewportHeight;

        if (!std::isfinite(result) || result <= 0.0)
            return fallback;

        return result;
    }

    glm::vec4 tacticalObjectColor(
        const world::celestial::SystemMapObject& object
    )
    {
        if (object.stableId == "player")
            return glm::vec4(1.00f, 0.78f, 0.28f, 0.98f);
        if (object.kind == world::celestial::SystemMapObjectKind::Hub)
            return glm::vec4(0.34f, 0.88f, 1.00f, 0.96f);
        return glm::vec4(0.78f, 0.86f, 0.94f, 0.96f);
    }

    MapObjectGlyphKind tacticalGlyphKind(
        world::celestial::SystemMapObjectKind kind
    )
    {
        if (kind == world::celestial::SystemMapObjectKind::Ship)
            return MapObjectGlyphKind::Ship;
        if (kind == world::celestial::SystemMapObjectKind::Hub)
            return MapObjectGlyphKind::Hub;
        return MapObjectGlyphKind::Infrastructure;
    }

    const char* celestialTypeName(world::celestial::BodyType type)
    {
        switch (type)
        {
            case world::celestial::BodyType::Planet: return "Planet";
            case world::celestial::BodyType::Moon: return "Moon";
            case world::celestial::BodyType::Star: return "Star";
            case world::celestial::BodyType::AsteroidBelt: return "Asteroid belt";
            default: return "Celestial body";
        }
    }
}

SystemMapSceneFrame SystemMapSceneFrameBuilder::build(
    const SystemMapView& viewState,
    const SystemMapRenderContext& context,
    const Viewport& viewport,
    const world::celestial::SystemMapSnapshot& system,
    const SystemMapPresentation& presentation
) const
{
    using world::celestial::BodyType;

    SystemMapSceneFrame frame;
    frame.valid = true;
    frame.systemId = system.systemId;
    frame.viewport = viewport;
    frame.systemScale = presentation.systemScale;
    frame.camera = viewState.cameraSnapshot(viewport);
    frame.cameraOrigin = frame.camera.targetAbsolute;
    frame.projection = frame.camera.projection;
    frame.view = frame.camera.view;
    frame.mvp = frame.camera.mvp;
    frame.worldUnitsPerPixel =
        frame.camera.worldUnitsPerPixel;

    const auto auToMapUnits =
        [&](const glm::dvec3& au) -> glm::dvec3
        {
            return glm::dvec3(
                au.x * static_cast<double>(frame.systemScale),
                au.y * static_cast<double>(frame.systemScale),
                au.z * static_cast<double>(frame.systemScale)
            );
        };

    const auto toRenderPosition =
        [&](const glm::dvec3& absolute) -> glm::vec3
        {
            return glm::vec3(absolute - frame.cameraOrigin);
        };

    std::unordered_map<std::string, double>
        bodyWorldUnitsPerPixelById;

    frame.bodyVisualPositionById.reserve(
        presentation.bodies.size()
    );
    frame.bodyVisualRadiusById.reserve(
        presentation.bodies.size()
    );
    frame.bodyVisualMetricsById.reserve(
        presentation.bodies.size()
    );
    frame.bodySelectionRadiusById.reserve(
        presentation.bodies.size()
    );

    for (const auto& body : presentation.bodies)
    {
        const glm::dvec3 absolute =
            auToMapUnits(body.positionAu);

        frame.interaction.bodyAbsolutePositionById[body.id] =
            absolute;
        frame.bodyVisualPositionById[body.id] =
            toRenderPosition(absolute);

        const float radius =
            context.bodyVisualRadius(
                body,
                frame.systemScale
            );

        frame.bodyVisualRadiusById[body.id] = radius;
        frame.interaction.bodyPhysicalRadiusWorldById[body.id] =
            radius;
    }

    for (const auto& body : presentation.bodies)
    {
        const auto positionIt =
            frame.bodyVisualPositionById.find(body.id);

        if (positionIt == frame.bodyVisualPositionById.end())
            continue;

        const double bodyWorldUnitsPerPixel =
            calculatePerspectiveWorldUnitsPerPixel(
                positionIt->second,
                frame.view,
                viewState.visuals().projectionFieldOfViewDeg,
                viewport.height,
                frame.worldUnitsPerPixel
            );

        bodyWorldUnitsPerPixelById[body.id] =
            bodyWorldUnitsPerPixel;

        frame.bodyVisualMetricsById[body.id] =
            context.computeSystemBodyVisualMetrics(
                body,
                frame.bodyVisualRadiusById[body.id],
                bodyWorldUnitsPerPixel
            );
    }

    for (const auto& body : presentation.bodies)
    {
        const auto metricsIt =
            frame.bodyVisualMetricsById.find(body.id);
        const auto positionIt =
            frame.bodyVisualPositionById.find(body.id);

        if (metricsIt == frame.bodyVisualMetricsById.end() ||
            positionIt == frame.bodyVisualPositionById.end())
        {
            continue;
        }

        const auto& metrics = metricsIt->second;

        const float visiblePickRadiusPx =
            std::max({
                metrics.physicalRadiusPx,
                metrics.drawMarker
                    ? metrics.markerRadiusPx * metrics.markerAlpha
                    : 0.0f,
                metrics.drawPointProxy
                    ? metrics.pointProxyRadiusPx * metrics.pointProxyAlpha
                    : 0.0f
            });

        const float pickRadiusPx =
            std::max(
                visiblePickRadiusPx,
                viewState.controls().pickMinBodyRadiusPx
            );

        const auto scaleIt =
            bodyWorldUnitsPerPixelById.find(body.id);

        const double bodyWorldUnitsPerPixel =
            scaleIt != bodyWorldUnitsPerPixelById.end()
                ? scaleIt->second
                : frame.worldUnitsPerPixel;

        frame.bodySelectionRadiusById[body.id] =
            std::max(
                metrics.physicalRadiusWorld,
                static_cast<float>(
                    bodyWorldUnitsPerPixel *
                    static_cast<double>(pickRadiusPx)
                )
            );

        if (body.type == BodyType::Star ||
            body.type == BodyType::Planet ||
            body.type == BodyType::Moon)
        {
            SystemMapBodyScreenPoint point;
            point.bodyId = body.id;
            point.name = body.name;
            point.screenRadiusPx = pickRadiusPx;
            point.physicalSizeMeters =
                std::max(0.0, body.radiusKm) * 2000.0;
            point.screen =
                context.projectToScreen(
                    positionIt->second,
                    frame.mvp,
                    viewport,
                    point.visible,
                    point.depth
                );

            const glm::vec2 bodyScreen = point.screen;
            const bool bodyVisible = point.visible;

            frame.interaction.bodyScreenPoints.push_back(
                std::move(point)
            );

            MapObjectOverlayItem bodyInfo;
            bodyInfo.objectId =
                "body:" + std::to_string(system.systemId) + ":" + body.id;
            bodyInfo.semanticTargetId = body.id;
            bodyInfo.trackingSystemId = system.systemId;
            bodyInfo.name = body.name;
            bodyInfo.typeName = celestialTypeName(body.type);
            bodyInfo.infoKind = MapObjectInfoKind::Celestial;
            bodyInfo.kind = MapObjectGlyphKind::Infrastructure;
            bodyInfo.drawGlyph = false;
            bodyInfo.pointerInteractive = false;
            bodyInfo.visible = bodyVisible;
            bodyInfo.screenPx = glm::dvec2(bodyScreen);
            bodyInfo.hitRadiusPx = 0.0;
            bodyInfo.physicalSizeMeters =
                std::max(1.0, body.radiusKm * 2000.0);
            bodyInfo.factionColor = body.color;
            bodyInfo.extraFields.push_back({
                "radius",
                std::to_string(static_cast<long long>(std::llround(body.radiusKm))),
                "km"
            });
            bodyInfo.trackingWorldPosition =
                world::coordinates::makeWorldPositionFromMeters(
                    system.systemPositionLy *
                        world::coordinates::MetersPerLightYear +
                    body.positionAu * world::celestial::MetersPerAu
                );
            bodyInfo.hasTrackingWorldPosition = true;
            frame.interaction.objectOverlay.items.push_back(
                std::move(bodyInfo)
            );
        }

        if (body.type == BodyType::Star ||
            body.type == BodyType::Planet ||
            body.type == BodyType::Moon)
        {
            SystemMapOrbitPivotScreenPoint point;
            point.bodyId = body.id;
            point.screenRadiusPx =
                std::max(visiblePickRadiusPx, 1.0f);
            point.screen =
                context.projectToScreen(
                    positionIt->second,
                    frame.mvp,
                    viewport,
                    point.visible,
                    point.depth
                );

            const glm::vec4 cameraSpace =
                frame.view *
                glm::vec4(positionIt->second, 1.0f);

            point.cameraDepthWorld =
                -static_cast<double>(cameraSpace.z);

            frame.interaction.orbitPivotScreenPoints.push_back(
                std::move(point)
            );
        }
    }

    for (const auto& object : system.objects)
    {
        const glm::dvec3 absolute =
            auToMapUnits(object.positionAu);
        const glm::vec3 visual =
            toRenderPosition(absolute);
        const std::string key =
            systemMapObjectStableKey(object);

        frame.objectVisualPositionById[key] = visual;
        frame.interaction.objectAbsolutePositionById[key] =
            absolute;

        MapObjectOverlayItem overlay;
        overlay.objectId = key;
        overlay.name = object.name;
        overlay.typeName = object.typeName.empty()
            ? (object.kind == world::celestial::SystemMapObjectKind::Ship
                ? "Ship"
                : object.kind == world::celestial::SystemMapObjectKind::Hub
                    ? "Hub"
                    : "Infrastructure")
            : object.typeName;
        overlay.owner = object.owner;
        overlay.navigationHubId =
            object.kind == world::celestial::SystemMapObjectKind::Hub
                ? key
                : object.parentHubId;
        overlay.navigationHubParentBodyId = object.parentBodyId;
        overlay.navigationSystemPositionAu = object.positionAu;
        overlay.hasNavigationSystemPositionAu = true;
        overlay.trackingWorldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                system.systemPositionLy *
                    world::coordinates::MetersPerLightYear +
                object.positionAu * world::celestial::MetersPerAu
            );
        overlay.hasTrackingWorldPosition = true;
        overlay.kind = tacticalGlyphKind(object.kind);
        overlay.velocityMode = MapObjectVelocityMode::Global;
        overlay.arrowVelocityMode = MapObjectVelocityMode::Global;
        overlay.displayedVelocityMps = object.velocityMps;
        overlay.velocityArrowMps = object.velocityMps;
        overlay.stellarVelocityMps = object.velocityMps;
        overlay.factionColor = tacticalObjectColor(object);
        overlay.physicalSizeMeters = std::max({
            std::abs(object.sizeMeters.x),
            std::abs(object.sizeMeters.y),
            std::abs(object.sizeMeters.z),
            1.0
        });

        float overlayDepth = 1.0f;
        overlay.screenPx = glm::dvec2(
            context.projectToScreen(
                visual,
                frame.mvp,
                viewport,
                overlay.visible,
                overlayDepth
            )
        );

        const double pixelsPerMeter =
            frame.worldUnitsPerPixel > 0.0
                ? (static_cast<double>(frame.systemScale) /
                   world::celestial::MetersPerAu) /
                    frame.worldUnitsPerPixel
                : 0.0;
        overlay.glyphScale = mapObjectGlyphScale(
            overlay.physicalSizeMeters,
            pixelsPerMeter
        );
        overlay.hitRadiusPx = 15.0 * overlay.glyphScale;

        const auto projectedDirection =
            [&](const glm::dvec3& direction) -> glm::dvec2
            {
                if (glm::length(direction) <= 1.0e-12)
                    return glm::dvec2(0.0, -1.0);
                const glm::vec3 endpoint =
                    visual +
                    glm::vec3(glm::normalize(direction)) *
                    static_cast<float>(frame.worldUnitsPerPixel * 30.0);
                bool endVisible = false;
                float endDepth = 1.0f;
                const glm::vec2 end = context.projectToScreen(
                    endpoint,
                    frame.mvp,
                    viewport,
                    endVisible,
                    endDepth
                );
                (void)endVisible;
                (void)endDepth;
                return normalizedScreenDirection(
                    glm::dvec2(end) - overlay.screenPx
                );
            };

        overlay.facingScreenDirection =
            projectedDirection(object.forwardWorld);
        overlay.velocityScreenDirection =
            projectedDirection(object.velocityMps);
        frame.interaction.objectOverlay.items.push_back(
            std::move(overlay)
        );

        if (object.kind !=
            world::celestial::SystemMapObjectKind::Hub)
        {
            continue;
        }

        SystemMapHubScreenPoint point;
        point.hubId = key;
        point.parentBodyId = object.parentBodyId;
        point.name = object.name;
        point.physicalSizeMeters = std::max({
            std::abs(object.sizeMeters.x),
            std::abs(object.sizeMeters.y),
            std::abs(object.sizeMeters.z),
            1.0
        });
        point.screen =
            context.projectToScreen(
                visual,
                frame.mvp,
                viewport,
                point.visible,
                point.depth
            );
        point.screenRadiusPx = 15.0f;

        frame.interaction.hubScreenPoints.push_back(
            std::move(point)
        );
    }

    return frame;
}
}
