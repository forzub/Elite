#include "src/game/system_map/GalaxyMapView.h"

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/world/coordinates/WorldPosition.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
    glm::vec3 orbitCameraDirectionFromYawPitch(
        float yaw,
        float pitch
    )
    {
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);

        return glm::vec3(
            cp * sy,
            sp,
            cp * cy
        );
    }

    glm::vec3 orbitCameraUpFromYawPitch(
        float yaw,
        float pitch
    )
    {
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);

        glm::vec3 up(
            -sp * sy,
            cp,
            -sp * cy
        );

        if (glm::length(up) < 0.000001f)
            return glm::vec3(0.0f, 1.0f, 0.0f);

        return glm::normalize(up);
    }
}

namespace game::system_map
{
    GalaxyMapView::GalaxyMapView()
    {
        reset();
    }

    void GalaxyMapView::reset()
    {
        m_state.camera = GalaxyCameraState {};
        m_state.camera.distance = m_visuals.initialCameraDistance;
        m_state.cameraFlight.reset();
        m_state.navigationGrid.reset();

        m_state.navigationFocusLy = glm::dvec3(0.0);
        m_state.navigationFocusValid = false;

        m_state.hoverVisualCell.reset();
        m_state.hoverVisualAlpha = 0.0f;
        m_state.hoverOutgoingCell.reset();
        m_state.hoverOutgoingAlpha = 0.0f;
        m_state.hoverVisualLastTimeSeconds = 0.0;
        m_state.cubeClickTracker.reset();

        m_state.entry = GalaxyMapEntryState {};
        m_state.selectedSystemId = -1;
        m_state.focusedSystemId = -1;
        m_state.screenPoints.clear();

        m_state.orbitPivotWorld = glm::vec3(0.0f);
        m_state.orbitPivotActive = false;
        m_state.mouseDownX = 0.0;
        m_state.mouseDownY = 0.0;
    }


    glm::dvec3 GalaxyMapView::playerPositionLy(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& playerNavigation,
        bool& outInsideKnownSystem
    ) const
    {
        const auto system =
            std::find_if(
                galaxy.systems.begin(),
                galaxy.systems.end(),
                [&](const auto& candidate)
                {
                    return
                        candidate.id ==
                        playerNavigation.currentSystemId;
                }
            );

        outInsideKnownSystem =
            system != galaxy.systems.end();

        if (outInsideKnownSystem)
        {
            return
                system->positionLy +
                playerNavigation.systemLocalAu /
                    navigation::SystemNavigationGrid::AuPerLightYear;
        }

        return world::coordinates::toGalacticLy(
            playerNavigation.worldPosition
        );
    }

    void GalaxyMapView::synchronizeCatalogRoots(
        const world::celestial::GalaxyMapSnapshot& galaxy
    )
    {
        std::vector<glm::dvec3> positionsLy;
        positionsLy.reserve(galaxy.systems.size());

        for (const auto& system : galaxy.systems)
            positionsLy.push_back(system.positionLy);

        m_state.navigationGrid.synchronizeCatalogPositions(
            positionsLy
        );
    }

    void GalaxyMapView::onEntered(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& playerNavigation
    )
    {
        synchronizeCatalogRoots(galaxy);

        /*
            Persistent camera state is preserved while the player remains in
            the same terminal Galaxy region. Transient interaction state must
            never survive closing the map or returning from a child map.
        */
        m_state.navigationGrid.clearHoveredCell();
        m_state.hoverVisualCell.reset();
        m_state.hoverVisualAlpha = 0.0f;
        m_state.hoverOutgoingCell.reset();
        m_state.hoverOutgoingAlpha = 0.0f;
        m_state.hoverVisualLastTimeSeconds = 0.0;
        m_state.cubeClickTracker.reset();

        bool insideKnownSystem = false;

        const glm::dvec3 playerLy =
            playerPositionLy(
                galaxy,
                playerNavigation,
                insideKnownSystem
            );

        m_state.entry.positionLy = playerLy;

        const int terminalLevel =
            m_state.navigationGrid.maximumLevel();

        const auto terminalCell =
            m_state.navigationGrid.nearestIndexForPositionLy(
                playerLy,
                terminalLevel
            );

        const int entrySystemId =
            insideKnownSystem
                ? playerNavigation.currentSystemId
                : -1;

        const bool playerRegionChanged =
            !m_state.entry.valid ||
            entrySystemId != m_state.entry.systemId ||
            terminalCell != m_state.entry.terminalCell;

        if (playerRegionChanged)
        {
            cancelCameraFlight(false);

            m_state.navigationGrid.reset();
            m_state.navigationGrid.setAnchorFromPositionLy(
                playerLy
            );
            m_state.navigationGrid.selectCell(
                m_state.navigationGrid.anchorCell()
            );

            m_state.navigationFocusLy = playerLy;
            m_state.navigationFocusValid = true;

            m_state.camera.target =
                positionLyToRender(
                    m_state.navigationGrid
                        .anchorCell()
                        .centerLy
                );

            const float initialCellEdgeRender =
                static_cast<float>(
                    m_state.navigationGrid
                        .anchorCell()
                        .sizeLy
                ) *
                RenderUnitsPerLightYear;

            m_state.camera.distance =
                std::clamp(
                    initialCellEdgeRender * 2.35f,
                    m_controls.minDistance,
                    m_controls.maxDistance
                );

            m_state.selectedSystemId = entrySystemId;
            m_state.focusedSystemId = entrySystemId;
        }

        m_state.entry.systemId = entrySystemId;
        m_state.entry.terminalCell = terminalCell;
        m_state.entry.valid = true;
    }

    void GalaxyMapView::resetNavigationToEntry()
    {
        cancelCameraFlight(false);

        m_state.navigationGrid.reset();
        m_state.navigationGrid.setAnchorFromPositionLy(
            m_state.entry.positionLy
        );
        m_state.navigationGrid.selectCell(
            m_state.navigationGrid.anchorCell()
        );
        m_state.navigationGrid.clearHoveredCell();

        m_state.camera = GalaxyCameraState {};

        m_state.camera.target =
            positionLyToRender(
                m_state.navigationGrid
                    .anchorCell()
                    .centerLy
            );

        const float initialCellEdgeRender =
            static_cast<float>(
                m_state.navigationGrid
                    .anchorCell()
                    .sizeLy
            ) *
            RenderUnitsPerLightYear;

        m_state.camera.distance =
            std::clamp(
                initialCellEdgeRender * 2.35f,
                m_controls.minDistance,
                m_controls.maxDistance
            );

        m_state.navigationFocusLy =
            m_state.entry.positionLy;
        m_state.navigationFocusValid = true;

        m_state.hoverVisualCell.reset();
        m_state.hoverVisualAlpha = 0.0f;
        m_state.hoverOutgoingCell.reset();
        m_state.hoverOutgoingAlpha = 0.0f;
        m_state.hoverVisualLastTimeSeconds = 0.0;
        m_state.cubeClickTracker.reset();
        m_state.orbitPivotActive = false;
    }

    glm::vec3 GalaxyMapView::positionLyToRender(
        const glm::dvec3& positionLy
    ) const
    {
        return glm::vec3(
            static_cast<float>(positionLy.x) * RenderUnitsPerLightYear,
            static_cast<float>(positionLy.y) * RenderUnitsPerLightYear,
            static_cast<float>(positionLy.z) * RenderUnitsPerLightYear
        );
    }

    glm::vec3 GalaxyMapView::vectorLyToRender(
        const glm::dvec3& vectorLy
    ) const
    {
        return glm::vec3(
            static_cast<float>(vectorLy.x) * RenderUnitsPerLightYear,
            static_cast<float>(vectorLy.y) * RenderUnitsPerLightYear,
            static_cast<float>(vectorLy.z) * RenderUnitsPerLightYear
        );
    }

    glm::dvec3 GalaxyMapView::renderToPositionLy(
        const glm::vec3& renderPosition
    ) const
    {
        const double inverseScale =
            1.0 / static_cast<double>(RenderUnitsPerLightYear);

        return glm::dvec3(
            static_cast<double>(renderPosition.x) * inverseScale,
            static_cast<double>(renderPosition.y) * inverseScale,
            static_cast<double>(renderPosition.z) * inverseScale
        );
    }

    glm::mat4 GalaxyMapView::viewMatrix() const
    {
        const glm::vec3 direction =
            orbitCameraDirectionFromYawPitch(
                m_state.camera.yaw,
                m_state.camera.pitch
            );

        const glm::vec3 up =
            orbitCameraUpFromYawPitch(
                m_state.camera.yaw,
                m_state.camera.pitch
            );

        const glm::vec3 eye =
            m_state.camera.target +
            direction * m_state.camera.distance;

        return glm::lookAt(
            eye,
            m_state.camera.target,
            up
        );
    }

    glm::mat4 GalaxyMapView::projectionMatrix(
        const Viewport& viewport
    ) const
    {
        const float aspect =
            viewport.height > 0
                ? static_cast<float>(viewport.width) /
                    static_cast<float>(viewport.height)
                : 1.0f;

        return glm::perspective(
            glm::radians(48.0f),
            aspect,
            0.0001f,
            2000.0f
        );
    }

    float GalaxyMapView::navigationAnchorDiameterPx(
        const Viewport& viewport
    ) const
    {
        if (!m_state.navigationGrid.enabled() ||
            viewport.height <= 0)
        {
            return 0.0f;
        }

        const float cellEdgeRender =
            static_cast<float>(
                m_state.navigationGrid.cellSizeLy()
            ) *
            RenderUnitsPerLightYear;

        const float viewLayerDepth =
            std::max(
                m_state.camera.distance,
                0.0001f
            );

        return
            navigation::cubicNavigationPerspectiveProjectedDiameterPx(
                cellEdgeRender,
                viewLayerDepth,
                glm::radians(48.0f),
                viewport.height
            );
    }

    bool GalaxyMapView::navigationCellsInteractive(
        const Viewport& viewport
    ) const
    {
        if (!m_state.navigationGrid.enabled() ||
            viewport.width <= 0 ||
            viewport.height <= 0)
        {
            return false;
        }

        const float viewportReferencePx =
            static_cast<float>(
                std::max(
                    1,
                    std::min(
                        viewport.width,
                        viewport.height
                    )
                )
            );

        const float minimumInteractiveDiameterPx =
            std::max(
                m_controls.navigationCellInteractiveMinPx,
                viewportReferencePx *
                    m_controls
                        .navigationCellInteractiveViewportFraction
            );

        return
            navigationAnchorDiameterPx(viewport) >=
            minimumInteractiveDiameterPx;
    }

    void GalaxyMapView::syncNavigationAnchorToCameraTarget()
    {
        if (!m_state.navigationGrid.enabled())
            return;

        m_state.navigationGrid.setAnchorFromPositionLy(
            renderToPositionLy(
                m_state.camera.target
            )
        );
    }

    void GalaxyMapView::beginCameraFlight(
        const glm::vec3& destinationTarget,
        float destinationDistance,
        double nowSeconds
    )
    {
        destinationDistance =
            std::clamp(
                destinationDistance,
                m_controls.minDistance,
                std::min(
                    m_controls.maxDistance,
                    m_visuals.labelMaxCameraDistance
                )
            );

        const float targetTravelDistance =
            glm::length(
                destinationTarget -
                m_state.camera.target
            );

        const float distanceChange =
            std::abs(
                destinationDistance -
                m_state.camera.distance
            );

        const float referenceDistance =
            std::max(
                1.0f,
                m_visuals.cameraFlightReferenceDistance
            );

        const float targetFactor =
            std::clamp(
                targetTravelDistance / referenceDistance,
                0.0f,
                1.0f
            );

        const float zoomFactor =
            std::clamp(
                distanceChange /
                    std::max(
                        0.0001f,
                        std::max(
                            m_state.camera.distance,
                            destinationDistance
                        )
                    ),
                0.0f,
                1.0f
            );

        const float duration =
            m_visuals.cameraFlightMinSeconds +
            (
                m_visuals.cameraFlightMaxSeconds -
                m_visuals.cameraFlightMinSeconds
            ) *
            std::max(targetFactor, zoomFactor);

        navigation::CubicNavigationCameraPose currentPose;
        currentPose.target = glm::dvec3(m_state.camera.target);
        currentPose.scale = static_cast<double>(m_state.camera.distance);

        navigation::CubicNavigationCameraPose destinationPose;
        destinationPose.target = glm::dvec3(destinationTarget);
        destinationPose.scale = static_cast<double>(destinationDistance);

        m_state.cameraFlight.begin(
            currentPose,
            destinationPose,
            nowSeconds,
            static_cast<double>(duration)
        );

        // begin() may complete immediately for a negligible move.
        m_state.camera.target = glm::vec3(currentPose.target);
        m_state.camera.distance = static_cast<float>(currentPose.scale);

        if (m_state.cameraFlight.active())
        {
            m_state.camera.rotating = false;
            m_state.camera.panning = false;
            m_state.orbitPivotActive = false;
        }
    }

    void GalaxyMapView::updateCameraFlight(
        double nowSeconds
    )
    {
        navigation::CubicNavigationCameraPose pose;
        pose.target = glm::dvec3(m_state.camera.target);
        pose.scale = static_cast<double>(m_state.camera.distance);

        if (!m_state.cameraFlight.update(nowSeconds, pose))
            return;

        m_state.camera.target = glm::vec3(pose.target);
        m_state.camera.distance = static_cast<float>(pose.scale);
    }

    void GalaxyMapView::cancelCameraFlight(
        bool snapToDestination
    )
    {
        navigation::CubicNavigationCameraPose pose;
        pose.target = glm::dvec3(m_state.camera.target);
        pose.scale = static_cast<double>(m_state.camera.distance);

        if (!m_state.cameraFlight.cancel(
                snapToDestination,
                pose
            ))
        {
            return;
        }

        m_state.camera.target = glm::vec3(pose.target);
        m_state.camera.distance = static_cast<float>(pose.scale);
    }

    MapIntent GalaxyMapView::entryIntentForPosition(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const glm::dvec3& positionLy,
        int explicitSystemId
    ) const
    {
        const int terminalLevel =
            m_state.navigationGrid.maximumLevel();

        const auto terminalIndex =
            m_state.navigationGrid.nearestIndexForPositionLy(
                positionLy,
                terminalLevel
            );

        if (!m_state.navigationGrid.isCellNavigable(
                terminalIndex,
                terminalLevel
            ))
        {
            return MapIntent {};
        }

        int entrySystemId = explicitSystemId;

        if (entrySystemId < 0 &&
            m_state.selectedSystemId >= 0)
        {
            const auto selectedSystem =
                std::find_if(
                    galaxy.systems.begin(),
                    galaxy.systems.end(),
                    [&](const auto& system)
                    {
                        return
                            system.id ==
                            m_state.selectedSystemId;
                    }
                );

            if (selectedSystem != galaxy.systems.end() &&
                m_state.navigationGrid.nearestIndexForPositionLy(
                    selectedSystem->positionLy,
                    terminalLevel
                ) == terminalIndex)
            {
                entrySystemId = selectedSystem->id;
            }
        }

        if (entrySystemId >= 0)
        {
            const auto explicitSystem =
                std::find_if(
                    galaxy.systems.begin(),
                    galaxy.systems.end(),
                    [&](const auto& system)
                    {
                        return system.id == entrySystemId;
                    }
                );

            if (explicitSystem != galaxy.systems.end())
            {
                return MapIntent::enterKnownSystem(
                    explicitSystem->id,
                    explicitSystem->positionLy
                );
            }
        }

        const auto terminalCell =
            m_state.navigationGrid.cell(
                terminalIndex,
                terminalLevel
            );

        return MapIntent::enterEmptySector(
            terminalCell.centerLy
        );
    }

    bool GalaxyMapView::focusSystem(
        int systemId,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        bool animateCamera,
        double nowSeconds
    )
    {
        const auto system =
            std::find_if(
                galaxy.systems.begin(),
                galaxy.systems.end(),
                [systemId](const auto& candidate)
                {
                    return candidate.id == systemId;
                }
            );

        if (system == galaxy.systems.end())
            return false;

        m_state.selectedSystemId = system->id;
        m_state.focusedSystemId = system->id;

        // Keep the exact star position as the navigation target.
        m_state.navigationFocusLy = system->positionLy;
        m_state.navigationFocusValid = true;

        m_state.navigationGrid.setAnchorFromPositionLy(
            system->positionLy
        );

        m_state.navigationGrid.selectCell(
            m_state.navigationGrid.anchorCell()
        );

        const glm::vec3 destinationTarget =
            positionLyToRender(system->positionLy);

        if (animateCamera)
        {
            beginCameraFlight(
                destinationTarget,
                m_state.camera.distance,
                nowSeconds
            );
        }
        else
        {
            m_state.camera.target = destinationTarget;
        }

        return true;
    }


}
