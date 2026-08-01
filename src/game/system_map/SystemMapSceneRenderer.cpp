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
#include "src/game/system_map/SystemMapPresentation.h"
#include "src/game/system_map/SystemMapSceneFrame.h"
#include "src/game/system_map/SystemMapRenderContext.h"
#include "src/game/system_map/SystemMapSceneRenderer.h"
#include "src/game/system_map/SystemMapView.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
namespace
{
    constexpr double SystemMapAuKm = 149597870.7;

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

}


void SystemMapSceneRenderer::render(
    const SystemMapView& viewState,
    SystemMapRenderContext& context,
    const Viewport& vp,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::PlayerNavigationState& nav,
    const SystemMapPresentation& presentation,
    const SystemMapSceneFrame& frame
) const
{
    context.ensureSystemRenderResources();

    const auto& bodies = presentation.bodies;
    const float systemScale = frame.systemScale;
    const glm::mat4& view = frame.view;
    const glm::mat4& mvp = frame.mvp;
    const double systemWorldUnitsPerPixel =
        frame.worldUnitsPerPixel;
    const glm::dvec3& systemCameraOrigin =
        frame.cameraOrigin;

    const auto& drawRadiusById =
        frame.bodyVisualRadiusById;
    const auto& selectionRadiusById =
        frame.bodySelectionRadiusById;
    const auto& posById =
        frame.bodyVisualPositionById;
    const auto& presentationById =
        frame.bodyVisualMetricsById;
    const auto& objectVisualPosById =
        frame.objectVisualPositionById;

    using world::celestial::BodyType;

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

    const auto auToMapUnits =
        [&](const glm::dvec3& au) -> glm::dvec3
        {
            return glm::dvec3(
                au.x * static_cast<double>(systemScale),
                au.y * static_cast<double>(systemScale),
                au.z * static_cast<double>(systemScale)
            );
        };

    const auto toRenderPos =
        [&](const glm::dvec3& absoluteMapUnits) -> glm::vec3
        {
            return glm::vec3(
                absoluteMapUnits - systemCameraOrigin
            );
        };

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
                posById.at(body.id),
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
            posById.at(body->id),
            presentationById.at(body->id),
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
            posById.at(body->id),
            presentationById.at(body->id),
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
            posById.at(body.id),
            presentationById.at(body.id),
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
            posById.at(body.id),
            presentationById.at(body.id),
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
                std::find_if(
                    bodies.begin(),
                    bodies.end(),
                    [&](const auto& body)
                    {
                        return
                            body.id ==
                            viewState.state().selectedBodyId;
                    }
                );

            if (selectedBodyIt != bodies.end())
            {
                haloColor =
                    context.colorForBodyType(
                        selectedBodyIt->type
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
        const double kmPerPixel =
            systemWorldUnitsPerPixel /
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
