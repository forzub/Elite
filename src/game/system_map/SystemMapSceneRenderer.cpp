/*
    System map scene orchestration.

    This standalone translation unit is independent of SystemMapRenderer and
    communicates only through SystemMapRenderContext.
*/

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/system_map/SystemMapFrameData.h"
#include "src/game/system_map/SystemMapRenderContext.h"
#include "src/game/system_map/SystemMapSceneRenderer.h"
#include "src/game/system_map/SystemMapView.h"
#include "src/world/celestial/CelestialOrbitKinematics.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
namespace
{
    constexpr double SystemMapAuKm = 149597870.7;

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
        const glm::vec3& renderPosition,
        const glm::mat4& view,
        float fieldOfViewDeg,
        int viewportHeight,
        double fallbackWorldUnitsPerPixel
    )
    {
        const glm::vec4 cameraSpace =
            view *
            glm::vec4(
                renderPosition,
                1.0f
            );

        const double cameraDepth =
            -static_cast<double>(cameraSpace.z);

        if (!std::isfinite(cameraDepth) ||
            cameraDepth <= 0.000000001)
        {
            return fallbackWorldUnitsPerPixel;
        }

        const double halfFovRad =
            glm::radians(
                static_cast<double>(fieldOfViewDeg) *
                0.5
            );

        const double safeViewportHeight =
            static_cast<double>(
                std::max(viewportHeight, 1)
            );

        const double localWorldUnitsPerPixel =
            2.0 *
            cameraDepth *
            std::tan(halfFovRad) /
            safeViewportHeight;

        if (!std::isfinite(localWorldUnitsPerPixel) ||
            localWorldUnitsPerPixel <= 0.0)
        {
            return fallbackWorldUnitsPerPixel;
        }

        return localWorldUnitsPerPixel;
    }

    double niceScaleNumber(double value)
    {
        if (value <= 0.0 || !std::isfinite(value))
            return 1.0;

        const double exponent =
            std::floor(std::log10(value));
        const double base =
            std::pow(10.0, exponent);
        const double normalized = value / base;

        double nice = 1.0;

        if (normalized <= 1.0)
            nice = 1.0;
        else if (normalized <= 2.0)
            nice = 2.0;
        else if (normalized <= 5.0)
            nice = 5.0;
        else
            nice = 10.0;

        return nice * base;
    }

    std::string formatScaleDistance(double km)
    {
        std::ostringstream text;

        if (km >= SystemMapAuKm * 0.1)
        {
            text << std::fixed << std::setprecision(3)
                 << (km / SystemMapAuKm)
                 << " AU";
        }
        else if (km >= 1000000.0)
        {
            text << std::fixed << std::setprecision(2)
                 << (km / 1000000.0)
                 << " M km";
        }
        else if (km >= 1000.0)
        {
            text << std::fixed << std::setprecision(0)
                 << km
                 << " km";
        }
        else
        {
            text << std::fixed << std::setprecision(0)
                 << km
                 << " km";
        }

        return text.str();
    }

    std::string systemObjectStableKey(
        const world::celestial::SystemMapObject& object
    )
    {
        if (!object.stableId.empty())
            return object.stableId;

        return
            "entity:" +
            std::to_string(object.id.value);
    }
}


double SystemMapSceneRenderer::resolvePresentationTimeSeconds(
    SystemMapView& viewState,
    const world::celestial::SystemMapSnapshot& system,
    double wallNowSeconds
) const
{
    const bool sourceChanged =
        viewState.state().presentationSystemId !=
            system.systemId ||
        std::abs(
            viewState.state().presentationSourceTimeSeconds -
                system.universeTimeSeconds
        ) > 0.000001 ||
        std::abs(
            viewState.state().presentationTimeScale -
                system.universeTimeScale
        ) > 0.000001;

    if (sourceChanged)
    {
        viewState.state().presentationSystemId =
            system.systemId;

        viewState.state().presentationSourceTimeSeconds =
            system.universeTimeSeconds;

        viewState.state().presentationWallTimeSeconds =
            wallNowSeconds;

        viewState.state().presentationTimeScale =
            std::max(
                0.0,
                system.universeTimeScale
            );
    }

    return
        viewState.state().presentationSourceTimeSeconds +
        std::max(
            0.0,
            wallNowSeconds -
                viewState.state().presentationWallTimeSeconds
        ) *
        viewState.state().presentationTimeScale;
}

void SystemMapSceneRenderer::render(
    SystemMapView& viewState,
    SystemMapRenderContext& context,
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
) const
{
    context.ensureSystemRenderResources();

    auto& frame = context.systemFrameData();
    frame.clearPresentation();


    const bool navigationSystemChanged =
        viewState.state().navigationGrid.systemId() !=
            system.systemId;

    if (navigationSystemChanged)
    {
        viewState.state().navigationGrid.activateSystem(
            system.systemId
        );

        viewState.state().hoverVisualCell.reset();
        viewState.state().hoverVisualAlpha = 0.0f;
        viewState.state().hoverOutgoingCell.reset();
        viewState.state().hoverOutgoingAlpha = 0.0f;
        viewState.state().hoverVisualLastTimeSeconds = 0.0;
        viewState.state().cubeClickTracker.reset();
        viewState.state().navigationCellExplicitlySelected = false;
        viewState.state().selectedBodyId.clear();
        viewState.state().selectedHubId.clear();
        viewState.state().selectedHubParentBodyId.clear();
    }

    const double presentationTimeSeconds =
        resolvePresentationTimeSeconds(
            viewState,
            system,
            context.currentTimeSeconds()
        );

    /*
        Build a presentation snapshot from the last authoritative server
        snapshot. The shared orbit equations are the same ones used by
        CelestialSystemRuntime, so accelerated time remains continuous
        without creating a second independent simulation.
    */
    std::vector<
        world::celestial::SystemMapBody
    > visualBodies =
        system.bodies;

    std::unordered_map<
        std::string,
        glm::dvec3
    > visualBodyPositionAuById;

    for (auto& body : visualBodies)
    {
        glm::dvec3 visualOrbitCenter =
            body.orbitCenterAu;

        const auto parentIt =
            visualBodyPositionAuById.find(
                body.parentId
            );

        if (parentIt !=
            visualBodyPositionAuById.end())
        {
            visualOrbitCenter =
                parentIt->second;
        }

        if (body.drawOrbit &&
            body.orbitRadiusAu > 0.0 &&
            body.orbitalPeriodDays > 0.0)
        {
            const double phaseRad =
                world::celestial::
                    circularOrbitPhaseRad(
                        presentationTimeSeconds,
                        body.orbitalPeriodDays,
                        body.orbitalDirection,
                        body.orbitalPhaseOffsetRad
                    );

            body.positionAu =
                visualOrbitCenter +
                world::celestial::
                    circularOrbitPositionAu(
                        body.orbitRadiusAu,
                        phaseRad
                    );
        }
        else if (parentIt !=
                 visualBodyPositionAuById.end())
        {
            /*
                Preserve a static relative offset if a child has no period.
            */
            body.positionAu =
                visualOrbitCenter +
                (
                    body.positionAu -
                    body.orbitCenterAu
                );
        }

        body.orbitCenterAu =
            visualOrbitCenter;

        if (body.dayLengthHours > 0.0)
        {
            const double snapshotRotationOffset =
                body.rotationPhaseRad -
                static_cast<double>(
                    body.rotationDirection < 0
                        ? -1
                        : 1
                ) *
                std::fmod(
                    system.universeTimeSeconds /
                        (
                            body.dayLengthHours *
                            3600.0
                        ),
                    1.0
                ) *
                world::celestial::OrbitTwoPi;

            body.rotationPhaseRad =
                world::celestial::
                    bodyRotationPhaseRad(
                        presentationTimeSeconds,
                        body.dayLengthHours,
                        body.rotationDirection,
                        snapshotRotationOffset
                    );
        }

        visualBodyPositionAuById[body.id] =
            body.positionAu;
    }

    const auto& bodies =
        visualBodies;


    /*
        Пустой межзвёздный сектор всё равно должен рисовать
        System navigation grid.

        Для известной системы масштаб определяется орбитами.
        Для пустого сектора S0 вписывается целиком и становится
        исходным материнским кубом.
    */
    double maxAu =
        bodies.empty()
            ? std::max(
                1.0,
                viewState.state().navigationGrid.cellSize(
                    viewState.state().navigationGrid
                        .definition()
                        .minimumLevel
                ) * 0.5
            )
            : 1.0;

    for (const auto& b : bodies)
    {
        const double r =
            std::sqrt(
                b.positionAu.x * b.positionAu.x +
                b.positionAu.y * b.positionAu.y +
                b.positionAu.z * b.positionAu.z
            );

        maxAu = std::max(maxAu, r);

        if (b.drawOrbit &&
            b.orbitRadiusAu > 0.0)
        {
            const double orbitCenterRadius =
                std::sqrt(
                    b.orbitCenterAu.x * b.orbitCenterAu.x +
                    b.orbitCenterAu.y * b.orbitCenterAu.y +
                    b.orbitCenterAu.z * b.orbitCenterAu.z
                );

            maxAu =
                std::max(
                    maxAu,
                    orbitCenterRadius +
                        b.orbitRadiusAu
                );
        }

    }

    const float systemScale =
        viewState.controls().fittedSystemRadiusWorld /
        static_cast<float>(maxAu);

    viewState.state().lastScale = systemScale;

    /*
        Fit each system once. Returning from another map mode preserves the
        existing view, while opening a different system starts with readable
        orbits instead of inheriting an unrelated extreme zoom.
    */
    if (viewState.state().lastCameraFitSystemId != system.systemId)
    {
        viewState.cancelCameraFlight(false);

        viewState.state().camera.target =
            glm::dvec3(0.0);

        viewState.state().camera.distance =
            std::clamp(
                viewState.controls().fittedSystemRadiusWorld *
                    viewState.controls().initialFitPadding,
                SystemMapView::minimumCameraHalfHeight,
                SystemMapView::maximumCameraHalfHeight
            );

        viewState.state().navigationGrid.setAnchorFromPosition(
            glm::dvec3(0.0)
        );

        viewState.state().navigationGrid.selectCell(
            viewState.state().navigationGrid.anchorCell()
        );
        viewState.state().navigationCellExplicitlySelected = false;

        viewState.state().lastCameraFitSystemId =
            system.systemId;
    }

    const glm::mat4 proj =
        viewState.projectionMatrix(
            vp
        );

    const glm::mat4 view =
        viewState.viewMatrix();

    if (viewState.visuals().drawStarfield)
    {
        context.drawMapStarfield(
            vp,
            system.systemPositionLy,
            view,
            viewState.visuals().starfieldFieldOfViewDeg,
            viewState.visuals().starfieldSizeScale,
            false,
            viewState.visuals().starfieldBrightnessScale,
            viewState.visuals().milkyWayIntensityScale,
            viewState.visuals().milkyWayColorTint
        );
    }

    if (viewState.visuals().drawAtmosphereVeil)
    {
        context.drawMapAtmosphereVeil(
            viewState.visuals().atmosphereVeilCenterAlpha,
            viewState.visuals().atmosphereVeilEdgeAlpha,
            viewState.visuals().atmosphereVeilAquaStrength
        );
    }

    const glm::mat4 mvp =
        proj * view;

    const double systemWorldUnitsPerPixel =
        calculateSystemWorldUnitsPerPixel(
            static_cast<double>(
                viewState.state().camera.distance
            ),
            vp.height
        );

    using world::celestial::BodyType;

    // =========================================================
    // Static lookup data.
    // Эти таблицы нужны дальше для выбора, радиусов, орбит,
    // подписей и selection overlay.
    // =========================================================
    std::unordered_map<
        std::string,
        const world::celestial::SystemMapBody*
    > bodyById;

    std::unordered_map<
        std::string,
        float
    > drawRadiusById;




    std::unordered_map<
        std::string,
        float
    > selectionRadiusById;






    for (const auto& b : bodies)
    {
        bodyById[b.id] =
            &b;

        drawRadiusById[b.id] =
            context.bodyVisualRadius(
                b,
                systemScale
            );
    }

// Если выбранная цель исчезла или это звезда — сбрасываем выбор.
if (!viewState.state().selectedBodyId.empty())
{
    auto selectedIt =
        bodyById.find(
            viewState.state().selectedBodyId
        );

    if (selectedIt == bodyById.end() ||
        selectedIt->second->type == BodyType::Star)
    {
        viewState.state().selectedBodyId.clear();
    }
}

// =========================================================
// Floating origin для system map.
//
// absolutePosById хранит точные double-позиции в map units.
// posById хранит render-relative float-позиции около нуля.
// В GPU отдаём только relative position.
// =========================================================
std::unordered_map<
    std::string,
    glm::dvec3
> absolutePosById;

std::unordered_map<
    std::string,
    glm::vec3
> posById;

const glm::dvec3 systemCameraOrigin =
    viewState.state().camera.target;

auto auToMapUnits =
    [&](const glm::dvec3& au) -> glm::dvec3
    {
        return glm::dvec3(
            au.x * static_cast<double>(systemScale),
            au.y * static_cast<double>(systemScale),
            au.z * static_cast<double>(systemScale)
        );
    };

auto toRenderPos =
    [&](const glm::dvec3& absoluteMapUnits) -> glm::vec3
    {
        const glm::dvec3 relative =
            absoluteMapUnits -
            systemCameraOrigin;

        return glm::vec3(
            static_cast<float>(relative.x),
            static_cast<float>(relative.y),
            static_cast<float>(relative.z)
        );
    };

for (const auto& b : bodies)
{
    const glm::dvec3 absolutePos =
        auToMapUnits(
            b.positionAu
        );

    absolutePosById[b.id] =
        absolutePos;

    posById[b.id] =
        toRenderPos(
            absolutePos
        );
}


frame.bodyAbsolutePositionById = absolutePosById;
frame.bodyPhysicalRadiusWorldById = drawRadiusById;

/*
    Body selection is rendered by the body overlay only. Scene rendering must
    never rewrite CubicNavigationGrid::selectedCell; that cell is reserved for
    an explicit user cube selection.
*/















    std::unordered_map<
        std::string,
        SystemBodyVisualMetrics
    > presentationById;

    std::unordered_map<
        std::string,
        double
    > bodyWorldUnitsPerPixelById;

    presentationById.reserve(
        bodies.size()
    );

    bodyWorldUnitsPerPixelById.reserve(
        bodies.size()
    );

    for (const auto& body : bodies)
    {
        const double bodyWorldUnitsPerPixel =
            calculatePerspectiveWorldUnitsPerPixel(
                posById[body.id],
                view,
                viewState.visuals()
                    .projectionFieldOfViewDeg,
                vp.height,
                systemWorldUnitsPerPixel
            );

        bodyWorldUnitsPerPixelById[body.id] =
            bodyWorldUnitsPerPixel;

        presentationById[body.id] =
            context.computeSystemBodyVisualMetrics(
                body,
                drawRadiusById[body.id],
                bodyWorldUnitsPerPixel
            );
    }

    for (const auto& body : bodies)
    {
        const auto metricsIt =
            presentationById.find(
                body.id
            );

        if (metricsIt == presentationById.end())
            continue;

        const auto& metrics =
            metricsIt->second;

        const float visiblePickRadiusPx =
            std::max({
                metrics.physicalRadiusPx,
                metrics.drawMarker
                    ? metrics.markerRadiusPx *
                        metrics.markerAlpha
                    : 0.0f,
                metrics.drawPointProxy
                    ? metrics.pointProxyRadiusPx *
                        metrics.pointProxyAlpha
                    : 0.0f
            });

        const float pickRadiusPx =
            std::max(
                visiblePickRadiusPx,
                viewState.controls()
                    .pickMinBodyRadiusPx
            );

        const auto bodyScaleIt =
            bodyWorldUnitsPerPixelById.find(
                body.id
            );

        const double bodyWorldUnitsPerPixel =
            bodyScaleIt !=
                bodyWorldUnitsPerPixelById.end()
                ? bodyScaleIt->second
                : systemWorldUnitsPerPixel;

        selectionRadiusById[body.id] =
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
                    posById[body.id],
                    mvp,
                    vp,
                    point.visible,
                    point.depth
                );

            frame.bodyScreenPoints.push_back(
                std::move(point)
            );
        }

        if (body.type == BodyType::Star ||
            body.type == BodyType::Planet ||
            body.type == BodyType::Moon)
        {
            SystemMapOrbitPivotScreenPoint pivotPoint;
            pivotPoint.bodyId = body.id;
            pivotPoint.screenRadiusPx =
                std::max(
                    visiblePickRadiusPx,
                    1.0f
                );
            pivotPoint.screen =
                context.projectToScreen(
                    posById[body.id],
                    mvp,
                    vp,
                    pivotPoint.visible,
                    pivotPoint.depth
                );

            const glm::vec4 cameraSpacePosition =
                view *
                glm::vec4(
                    posById[body.id],
                    1.0f
                );

            pivotPoint.cameraDepthWorld =
                -static_cast<double>(
                    cameraSpacePosition.z
                );

            frame.orbitPivotScreenPoints.push_back(
                std::move(pivotPoint)
            );
        }
    }

    /* Base cartographic lines: grid, orbits, belts and player position. */
    context.beginLines();

    context.drawSystemNavigationGrid(
        vp,
        mvp,
        systemScale
    );

    for (const auto& body : bodies)
    {
        if (body.type != BodyType::Planet &&
            body.type != BodyType::AsteroidBelt)
        {
            continue;
        }

        if (!body.drawOrbit ||
            body.orbitRadiusAu <= 0.0)
        {
            continue;
        }

        const glm::vec3 orbitCenter =
            toRenderPos(
                auToMapUnits(
                    body.orbitCenterAu
                )
            );

        const float orbitRadius =
            static_cast<float>(
                body.orbitRadiusAu
            ) *
            systemScale;

        const glm::vec4 orbitColor =
            body.type == BodyType::AsteroidBelt
                ? viewState.visuals().scene
                    .asteroidBeltOrbitColor
                : viewState.visuals().scene
                    .planetOrbitColor;

        context.addCircleXZ(
            orbitCenter,
            orbitRadius,
            orbitColor,
            viewState.visuals().scene
                .primaryOrbitSegments
        );
    }

    for (const auto& body : bodies)
    {
        if (body.type != BodyType::AsteroidBelt)
            continue;

        const glm::vec3 beltCenter =
            toRenderPos(
                auToMapUnits(
                    body.orbitCenterAu
                )
            );

        const float beltRadius =
            static_cast<float>(
                body.orbitRadiusAu
            ) *
            systemScale;

        const float beltHalfWidth =
            std::max(
                0.12f,
                static_cast<float>(
                    systemWorldUnitsPerPixel * 2.0
                )
            );

        context.addCircleXZ(
            beltCenter,
            std::max(
                0.0f,
                beltRadius - beltHalfWidth
            ),
            glm::vec4(0.65f, 0.68f, 0.72f, 0.12f),
            160
        );

        context.addCircleXZ(
            beltCenter,
            beltRadius,
            glm::vec4(0.65f, 0.68f, 0.72f, 0.24f),
            160
        );

        context.addCircleXZ(
            beltCenter,
            beltRadius + beltHalfWidth,
            glm::vec4(0.65f, 0.68f, 0.72f, 0.12f),
            160
        );
    }

    if (system.systemId == nav.currentSystemId)
    {
        const glm::dvec3 playerAbsolute(
            nav.systemLocalAu.x *
                static_cast<double>(systemScale),
            nav.systemLocalAu.y *
                static_cast<double>(systemScale),
            nav.systemLocalAu.z *
                static_cast<double>(systemScale)
        );

        const glm::vec3 player =
            glm::vec3(
                playerAbsolute -
                systemCameraOrigin
            );

        context.addCross(
            player,
            static_cast<float>(
                systemWorldUnitsPerPixel * 10.0
            ),
            glm::vec4(1.0f, 0.82f, 0.35f, 1.0f)
        );

        context.addCircleXZ(
            player,
            static_cast<float>(
                systemWorldUnitsPerPixel * 17.0
            ),
            glm::vec4(1.0f, 0.82f, 0.35f, 0.55f),
            48
        );
    }

    std::unordered_map<std::string, glm::vec3>
        objectVisualPosById;

    frame.objectAbsolutePositionById.clear();

    for (const auto& object : system.objects)
    {
        const glm::dvec3 objectAbsolute =
            auToMapUnits(
                object.positionAu
            );

        const glm::vec3 objectPosition =
            toRenderPos(
                objectAbsolute
            );

        const std::string objectKey =
            systemObjectStableKey(
                object
            );

        objectVisualPosById[objectKey] =
            objectPosition;

        frame.objectAbsolutePositionById[objectKey] =
            objectAbsolute;

        if (object.kind !=
            world::celestial::SystemMapObjectKind::Hub)
        {
            continue;
        }

        SystemMapHubScreenPoint point;
        point.hubId = objectKey;
        point.parentBodyId = object.parentBodyId;
        point.name = object.name;
        point.screen =
            context.projectToScreen(
                objectPosition,
                mvp,
                vp,
                point.visible,
                point.depth
            );
        point.screenRadiusPx = 15.0f;

        frame.hubScreenPoints.push_back(
            std::move(point)
        );
    }

    context.flushLines(mvp);

    /*
        Explicit ring order:
            back half -> parent body -> front half -> satellites/other bodies

        The last step is intentional cartographic precedence: moons and their
        markers stay readable instead of being swallowed by a translucent
        ring sheet. The shared ring shader still uses the actual Jupiter,
        Saturn, Uranus or Neptune visual profile.
    */
    std::vector<const world::celestial::SystemMapBody*>
        ringedBodies;

    std::unordered_set<std::string>
        ringedBodyIds;

    for (const auto& body : bodies)
    {
        if (body.type == BodyType::AsteroidBelt)
            continue;

        const auto metricsIt =
            presentationById.find(
                body.id
            );

        if (metricsIt == presentationById.end())
            continue;

        if (context.renderSystemBodyRings(
                body,
                posById[body.id],
                metricsIt->second,
                view,
                mvp,
                vp,
                SystemMapRingPart::Back
            ))
        {
            ringedBodies.push_back(
                &body
            );

            ringedBodyIds.insert(
                body.id
            );
        }
    }

    context.beginSolids();
    context.beginTexturedBodies();

    for (const auto* body : ringedBodies)
    {
        context.addSystemBodyGeometry(
            *body,
            posById[body->id],
            presentationById[body->id],
            context.colorForBodyType(
                body->type
            ),
            view
        );
    }

    context.flushSolids(mvp);
    context.flushTexturedBodies(mvp);

    for (const auto* body : ringedBodies)
    {
        context.renderSystemBodyRings(
            *body,
            posById[body->id],
            presentationById[body->id],
            view,
            mvp,
            vp,
            SystemMapRingPart::Front
        );
    }

    /*
        Moon orbits are cartographic overlays. Rendering them after
        ring sheets prevents Saturn/Jupiter rings from erasing the
        satellite hierarchy, while moon geometry is still drawn
        afterwards and remains visually dominant at its position.
    */
    context.beginLines();

    for (const auto& body : bodies)
    {
        if (body.type != BodyType::Moon ||
            !body.drawOrbit ||
            body.orbitRadiusAu <= 0.0)
        {
            continue;
        }

        const glm::vec3 orbitCenter =
            toRenderPos(
                auToMapUnits(
                    body.orbitCenterAu
                )
            );

        const float orbitRadius =
            static_cast<float>(
                body.orbitRadiusAu
            ) *
            systemScale;

        context.addCircleXZ(
            orbitCenter,
            orbitRadius,
            viewState.visuals().scene
                .moonOrbitColor,
            viewState.visuals().scene
                .moonOrbitSegments
        );
    }

    context.flushLines(mvp);

    context.beginSolids();
    context.beginTexturedBodies();

    for (const auto& body : bodies)
    {
        if (body.type == BodyType::AsteroidBelt ||
            ringedBodyIds.find(body.id) !=
                ringedBodyIds.end())
        {
            continue;
        }

        context.addSystemBodyGeometry(
            body,
            posById[body.id],
            presentationById[body.id],
            context.colorForBodyType(
                body.type
            ),
            view
        );
    }

    context.flushSolids(mvp);
    context.flushTexturedBodies(mvp);

    /* Proxy markers are overlays, never opaque celestial geometry. */
    context.beginLines();

    for (const auto& body : bodies)
    {
        if (body.type == BodyType::AsteroidBelt)
            continue;

        context.addSystemBodyMarker(
            body,
            posById[body.id],
            presentationById[body.id],
            context.colorForBodyType(
                body.type
            ),
            view
        );
    }

    context.flushLines(mvp);


    if (!viewState.state().selectedBodyId.empty())
    {
        auto posIt =
            posById.find(viewState.state().selectedBodyId);

        auto radiusIt =
            selectionRadiusById.find(viewState.state().selectedBodyId);

        if (posIt != posById.end() &&
                radiusIt != selectionRadiusById.end())
        {
            context.beginLines();

            const glm::vec3 selectedPos =
                posIt->second;

            const float selectedRadius =
                radiusIt->second;

            glm::vec4 haloColor(
                1.0f,
                0.78f,
                0.28f,
                1.0f
            );

            const auto selectedBodyIt =
                bodyById.find(
                    viewState.state().selectedBodyId
                );

            if (selectedBodyIt != bodyById.end())
            {
                haloColor =
                    context.colorForBodyType(
                        selectedBodyIt->second->type
                    );

                haloColor.a = 1.0f;
            }

            /*
                A selected planet gets a separate, visible halo around the
                sharp body marker. This mirrors selected stars in Galaxy.
            */
            context.addBillboardHalo(
                selectedPos,
                selectedRadius,
                4.60f,
                0.42f,
                haloColor,
                view,
                7,
                96
            );

            context.addCircleXZ(
                selectedPos,
                selectedRadius * 1.95f,
                viewState.visuals().scene.selectedRingColor,
                96
            );

            context.addCircleXY(
                selectedPos,
                selectedRadius * 2.10f,
                viewState.visuals().scene.selectedSecondaryRingColor,
                96
            );

            context.flushLines(mvp);
        }
    }

    if (!viewState.state().selectedHubId.empty())
    {
        const auto selectedHubPosition =
            objectVisualPosById.find(
                viewState.state().selectedHubId
            );

        if (selectedHubPosition !=
            objectVisualPosById.end())
        {
            context.beginLines();

            const float markerRadius =
                static_cast<float>(
                    systemWorldUnitsPerPixel *
                    18.0
                );

            context.addCircleXY(
                selectedHubPosition->second,
                markerRadius,
                viewState.visuals().scene.selectedHubRingColor,
                64
            );

            context.addCircleXZ(
                selectedHubPosition->second,
                markerRadius * 1.15f,
                viewState.visuals().scene.selectedHubSecondaryRingColor,
                64
            );

            context.flushLines(mvp);
        }
    }

    if (!viewState.state().selectedHubId.empty())
    {
        const auto selectedHubPosition =
            frame.objectAbsolutePositionById.find(
                viewState.state().selectedHubId
            );

        if (selectedHubPosition ==
            frame.objectAbsolutePositionById.end())
        {
            viewState.state().selectedHubId.clear();
            viewState.state().selectedHubParentBodyId.clear();
        }
    }



    context.drawSystemObjectOverlays(
        system,
        view,
        mvp,
        objectVisualPosById,
        posById,
        drawRadiusById,
        systemWorldUnitsPerPixel,
        systemScale
    );











    context.drawSystemLabels(
        vp,
        system,
        mvp,
        posById,
        presentationById
    );




    context.drawSystemObjectLabels(
        vp,
        system,
        mvp,
        view,
        objectVisualPosById,
        posById,
        drawRadiusById
    );




    {
        const double worldUnitsPerPixel =
            calculateSystemWorldUnitsPerPixel(
                static_cast<double>(viewState.state().camera.distance),
                vp.height
            );

        const double kmPerPixel =
            static_cast<double>(worldUnitsPerPixel) /
            static_cast<double>(systemScale) *
            SystemMapAuKm;

        if (kmPerPixel > 0.0 &&
            std::isfinite(kmPerPixel))
        {
            const double desiredBarPx =
                150.0;

            const double niceKm =
                niceScaleNumber(
                    kmPerPixel * desiredBarPx
                );

            const double barPx =
                niceKm / kmPerPixel;

            const float x0 =
                28.0f;

            const float scaleOverlay =
                std::clamp(
                    static_cast<float>(vp.height) /
                        1080.0f,
                    0.72f,
                    1.35f
                );

            const float y0 =
                static_cast<float>(vp.height) -
                96.0f * scaleOverlay;

            const float x1 =
                x0 +
                static_cast<float>(
                    std::clamp(
                        barPx,
                        48.0,
                        260.0
                    )
                );

            const glm::mat4 scaleOrtho =
                glm::ortho(
                    0.0f,
                    static_cast<float>(vp.width),
                    static_cast<float>(vp.height),
                    0.0f,
                    -1.0f,
                    1.0f
                );

            context.beginLines();

            const glm::vec4 scaleColor(
                0.48f,
                0.78f,
                1.0f,
                0.72f
            );

            context.addLine(
                glm::vec3(x0, y0, 0.0f),
                glm::vec3(x1, y0, 0.0f),
                scaleColor
            );

            context.addLine(
                glm::vec3(x0, y0 - 5.0f, 0.0f),
                glm::vec3(x0, y0 + 5.0f, 0.0f),
                scaleColor
            );

            context.addLine(
                glm::vec3(x1, y0 - 5.0f, 0.0f),
                glm::vec3(x1, y0 + 5.0f, 0.0f),
                scaleColor
            );

            context.flushLines(
                scaleOrtho
            );

            context.beginTextFrame(
                vp.width,
                vp.height
            );

            context.drawTextPx(
                formatScaleDistance(
                    niceKm
                ),
                x0,
                y0 - 24.0f,
                12,
                viewState.visuals().scene.scalePrimaryTextColor
            );

            std::ostringstream pxLabel;

            pxLabel
                << "1 px = "
                << formatScaleDistance(
                    kmPerPixel
                );

            context.drawTextPx(
                pxLabel.str(),
                x0,
                y0 + 10.0f,
                10,
                viewState.visuals().scene.scaleSecondaryTextColor
            );

            context.endTextFrame();
        }
    }








}
} // namespace game::system_map
