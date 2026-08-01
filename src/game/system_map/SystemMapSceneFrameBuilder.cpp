#include "src/game/system_map/SystemMapSceneFrameBuilder.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/system_map/SystemMapPresentation.h"
#include "src/game/system_map/SystemMapView.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
namespace
{
    double calculateSystemWorldUnitsPerPixel(
        double cameraHalfHeight,
        int viewportHeight
    )
    {
        const double safeHeight =
            static_cast<double>(
                std::max(viewportHeight, 1)
            );

        const double halfHeight =
            std::clamp(
                cameraHalfHeight,
                static_cast<double>(
                    SystemMapView::minimumCameraHalfHeight
                ),
                static_cast<double>(
                    SystemMapView::maximumCameraHalfHeight
                )
            );

        return (halfHeight * 2.0) / safeHeight;
    }

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
    frame.cameraOrigin = viewState.state().camera.target;
    frame.projection = viewState.projectionMatrix(viewport);
    frame.view = viewState.viewMatrix();
    frame.mvp = frame.projection * frame.view;
    frame.worldUnitsPerPixel =
        calculateSystemWorldUnitsPerPixel(
            static_cast<double>(
                viewState.state().camera.distance
            ),
            viewport.height
        );

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

        if (body.type == BodyType::Planet ||
            body.type == BodyType::Moon)
        {
            SystemMapBodyScreenPoint point;
            point.bodyId = body.id;
            point.name = body.name;
            point.screenRadiusPx = pickRadiusPx;
            point.screen =
                context.projectToScreen(
                    positionIt->second,
                    frame.mvp,
                    viewport,
                    point.visible,
                    point.depth
                );

            frame.interaction.bodyScreenPoints.push_back(
                std::move(point)
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

        if (object.kind !=
            world::celestial::SystemMapObjectKind::Hub)
        {
            continue;
        }

        SystemMapHubScreenPoint point;
        point.hubId = key;
        point.parentBodyId = object.parentBodyId;
        point.name = object.name;
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
