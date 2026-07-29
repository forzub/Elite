/*
    System map scene orchestration.

    Included from SystemMapRenderer.cpp during the incremental extraction.
    The class itself is independent of SystemMapRenderer and communicates
    only through SystemMapRenderContext.
*/

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

/*
    A selected moving body owns only its highlighted cell.
    It must not move the navigation anchor or camera behind the user's back.
*/
if (!viewState.state().selectedBodyId.empty())
{
    const auto selectedPositionIt =
        visualBodyPositionAuById.find(
            viewState.state().selectedBodyId
        );

    if (selectedPositionIt !=
        visualBodyPositionAuById.end())
    {
        const int selectedLevel =
            viewState.state().navigationGrid.level();

        const auto selectedIndex =
            viewState.state().navigationGrid
                .nearestIndexForPosition(
                    selectedPositionIt->second,
                    selectedLevel
                );

        viewState.state().navigationGrid.selectCell(
            viewState.state().navigationGrid.cell(
                selectedIndex,
                selectedLevel
            )
        );
    }
}















    context.beginLines();
    context.beginSolids();
    context.beginTexturedBodies();






    context.drawSystemNavigationGrid(
        vp,
        mvp,
        systemScale
    );



    // Орбиты планет, лун и астероидных поясов.
    for (const auto& b : bodies)
    {
        if (b.type != BodyType::Planet &&
            b.type != BodyType::Moon)
        {
            continue;
        }


        if (!b.drawOrbit || b.orbitRadiusAu <= 0.0)
            continue;




        const glm::dvec3 orbitCenterAbsolute =
            auToMapUnits(
                b.orbitCenterAu
            );

        const glm::vec3 center =
            toRenderPos(
                orbitCenterAbsolute
            );

        float orbitR =
            static_cast<float>(b.orbitRadiusAu) * systemScale;









        const glm::vec4 orbitColor =
            b.type == BodyType::Moon
                ? viewState.visuals().scene.moonOrbitColor
                : b.type == BodyType::AsteroidBelt
                    ? viewState.visuals().scene.asteroidBeltOrbitColor
                    : viewState.visuals().scene.planetOrbitColor;

        context.addCircleXZ(
            center,
            orbitR,
            orbitColor,
            b.type == BodyType::Moon
                ? viewState.visuals().scene.moonOrbitSegments
                : viewState.visuals().scene.primaryOrbitSegments
        );
    }









    // Тела, кольца, пояса.
    for (const auto& b : bodies)
    {
        const glm::vec3 p = posById[b.id];
        const glm::vec4 c = context.colorForBodyType(b.type);
        const float r = drawRadiusById[b.id];















       SystemBodyVisualMetrics bodyMetrics =
    context.computeSystemBodyVisualMetrics(
        b,
        r,
        systemWorldUnitsPerPixel
    );

if (b.type == BodyType::Moon &&
    bodyMetrics.drawMarker &&
    !bodyMetrics.drawPhysicalBody &&
    b.id != viewState.state().selectedBodyId &&
    !b.parentId.empty())
{
    const auto parentBodyIt =
        std::find_if(
            bodies.begin(),
            bodies.end(),
            [&](const auto& candidate)
            {
                return candidate.id == b.parentId;
            }
        );

    if (parentBodyIt != bodies.end())
    {
        const auto parentRadiusIt =
            drawRadiusById.find(
                parentBodyIt->id
            );

        if (parentRadiusIt != drawRadiusById.end())
        {
            const auto parentMetrics =
                context.computeSystemBodyVisualMetrics(
                    *parentBodyIt,
                    parentRadiusIt->second,
                    systemWorldUnitsPerPixel
                );

            if (parentMetrics.drawPhysicalBody &&
                parentMetrics.physicalRadiusPx >= 22.0f)
            {
                bodyMetrics.drawMarker = false;
                bodyMetrics.markerRadiusPx = 0.0f;
                bodyMetrics.markerRadiusWorld = 0.0f;
                bodyMetrics.pickRadiusPx =
                    std::max(
                        bodyMetrics.physicalRadiusPx,
                        viewState.controls().pickMinBodyRadiusPx
                    );
            }
        }
    }
}

const float selectionRadiusWorld =
    std::max(
        bodyMetrics.physicalRadiusWorld,
        static_cast<float>(
            systemWorldUnitsPerPixel *
            static_cast<double>(
                bodyMetrics.pickRadiusPx
            )
        )
    );

selectionRadiusById[b.id] =
    selectionRadiusWorld;

if (b.type == BodyType::Planet ||
    b.type == BodyType::Moon)
{
    SystemMapBodyScreenPoint bp;

    bp.bodyId = b.id;
    bp.name = b.name;

    // Это уже не "физический радиус диска".
    // Это интерактивный радиус выбора.
    bp.screenRadiusPx =
        bodyMetrics.pickRadiusPx;

    bp.screen =
        context.projectToScreen(
            p,
            mvp,
            vp,
            bp.visible,
            bp.depth
        );

    frame.bodyScreenPoints.push_back(
        bp
    );
}

if (b.type == BodyType::AsteroidBelt)
{
    const glm::dvec3 orbitCenterAbsolute =
        auToMapUnits(
            b.orbitCenterAu
        );

    const glm::vec3 center =
        toRenderPos(
            orbitCenterAbsolute
        );

    const float beltR =
        static_cast<float>(b.orbitRadiusAu) *
        systemScale;

    context.addCircleXZ(
        center,
        beltR - 0.12f,
        {0.65f, 0.68f, 0.72f, 0.12f},
        160
    );

    context.addCircleXZ(
        center,
        beltR,
        {0.65f, 0.68f, 0.72f, 0.24f},
        160
    );

    context.addCircleXZ(
        center,
        beltR + 0.12f,
        {0.65f, 0.68f, 0.72f, 0.12f},
        160
    );

    continue;
}

context.addSystemBodyRingVisuals(
    b,
    p,
    bodyMetrics,
    systemScale,
    systemWorldUnitsPerPixel,
    view
);

context.addSystemBodyVisual(
    b,
    p,
    bodyMetrics,
    c,
    view
);














    }

    if (system.systemId == nav.currentSystemId)
    {
        const glm::dvec3 playerAbsolute(
                nav.systemLocalAu.x * static_cast<double>(systemScale),
                nav.systemLocalAu.y * static_cast<double>(systemScale),
                nav.systemLocalAu.z * static_cast<double>(systemScale)
            );

        const glm::dvec3 playerRelative =
            playerAbsolute -
            systemCameraOrigin;

        glm::vec3 player {
            static_cast<float>(playerRelative.x),
            static_cast<float>(playerRelative.y),
            static_cast<float>(playerRelative.z)
        };

        const float playerCrossSize =
            systemWorldUnitsPerPixel *
            10.0f;

        const float playerCircleRadius =
            systemWorldUnitsPerPixel *
            17.0f;

        context.addCross(
            player,
            playerCrossSize,
            glm::vec4(1.0f, 0.82f, 0.35f, 1.0f)
        );

        context.addCircleXZ(
            player,
            playerCircleRadius,
            glm::vec4(1.0f, 0.82f, 0.35f, 0.55f),
            48
        );
    }


    std::unordered_map<std::string, glm::vec3> objectVisualPosById;
    frame.objectAbsolutePositionById.clear();










    for (const auto& obj : system.objects)
    {
        const glm::dvec3 objectAbsolute =
            auToMapUnits(
                obj.positionAu
            );

        const glm::vec3 p =
            toRenderPos(
                objectAbsolute
            );

        const std::string objectKey =
            systemObjectStableKey(
                obj
            );

        objectVisualPosById[objectKey] =
            p;
        frame.objectAbsolutePositionById[objectKey] =
            objectAbsolute;

        if (obj.kind ==
            world::celestial::
                SystemMapObjectKind::Hub)
        {
            SystemMapHubScreenPoint point;
            point.hubId = objectKey;
            point.parentBodyId = obj.parentBodyId;
            point.name = obj.name;
            point.screen =
                context.projectToScreen(
                    p,
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
    }







    context.flushLines(mvp);

    // Draw old solid bodies first.
    // Textured bodies are drawn after them.
    context.flushSolids(mvp);
    context.flushTexturedBodies(mvp);




    // Selection overlay must be drawn AFTER bodies,
    // otherwise the planet texture overpaints the marker.
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
        else
        {
            const glm::dvec3 hubPositionAu =
                selectedHubPosition->second /
                static_cast<double>(
                    viewState.state().lastScale
                );

            const int selectedLevel =
                viewState.state().navigationGrid.level();

            viewState.state().navigationGrid.selectCell(
                viewState.state().navigationGrid.cell(
                    viewState.state().navigationGrid
                        .nearestIndexForPosition(
                            hubPositionAu,
                            selectedLevel
                        ),
                    selectedLevel
                )
            );
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
        drawRadiusById
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
