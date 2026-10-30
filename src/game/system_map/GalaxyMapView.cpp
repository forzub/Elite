#include "src/game/system_map/GalaxyMapView.h"

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/world/coordinates/WorldPosition.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>


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

        m_state.entry = GalaxyMapEntryState {};
        m_state.selectedSystemId = -1;
        m_state.focusedSystemId = -1;

        m_state.hierarchyParent = GalaxyHierarchyParent {};
        m_state.hierarchyFaces = GalaxyHierarchyFaces {};
        m_state.hoverLayerAxis = -1;
        m_state.flightContextBox.reset();
        ++m_state.hierarchyRevision;

        m_state.orbitPivotWorld = glm::vec3(0.0f);
        resetTransientPointerState();
    }

    void GalaxyMapView::resetTransientPointerState()
    {
        m_state.cubeClickTracker.reset();
        m_state.mediumClickTracker.reset();
        m_state.starClickTracker.reset();
        m_state.hoveredMediumObject = -1;
        m_state.selectedMediumObject = -1;
        m_state.selectedMediumHitLy = glm::dvec3(0.0);
        m_state.hoverTarget = GalaxyPointerTarget {};
        m_state.hoverTargetValid = false;
        m_state.hoverMouseX = -1.0e9;
        m_state.hoverMouseY = -1.0e9;
        m_state.hoverHierarchyRevision = -1;
        m_state.whatIsHere = GalaxyWhatIsHereMenu {};
        m_state.presentedStars.clear();
        m_state.presentedCubeMarkers.clear();
        m_state.pointerGesture.reset();
        m_state.pressTarget = GalaxyPointerTarget {};
        m_state.pressResolution = GalaxyPointerResolution {};
        m_state.orbitPivotActive = false;
        m_state.camera.rotating = false;
        m_state.camera.panning = false;
    }


    void GalaxyMapView::suppressCameraGesture(
        bool leftDown,
        bool rightDown,
        double mouseX,
        double mouseY
    )
    {
        m_state.camera.rotating = false;
        m_state.camera.panning = false;
        m_state.camera.leftWasDown = leftDown;
        m_state.camera.rightWasDown = rightDown;
        m_state.camera.lastMouseX = mouseX;
        m_state.camera.lastMouseY = mouseY;
        // The gesture belongs to whoever consumed the press (overlay, UI):
        // no map click / drag may follow from it.
        m_state.pointerGesture.suppress(leftDown);
        m_state.orbitPivotActive = false;
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

        m_state.navigationGrid.extendDomainTo(
            positionsLy
        );

        // The overview frames the roots holding star systems (+ the
        // player's root, added on entry).
        auto& roots = m_state.overviewRoots;
        const std::size_t before = roots.size();
        const auto addRoot = [&](const glm::dvec3& positionLy) {
            const auto index = m_state.navigationGrid.nearestIndexForPositionLy(positionLy, 0);
            if (!m_state.navigationGrid.isCellNavigable(index, 0))
                return;
            if (std::find(roots.begin(), roots.end(), index) == roots.end())
                roots.push_back(index);
        };
        for (const auto& positionLy : positionsLy)
            addRoot(positionLy);
        if (m_state.entry.valid)
            addRoot(m_state.entry.positionLy);
        if (roots.size() != before)
            ++m_state.hierarchyRevision;
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
        resetTransientPointerState();

        bool insideKnownSystem = false;

        const glm::dvec3 playerLy =
            playerPositionLy(
                galaxy,
                playerNavigation,
                insideKnownSystem
            );

        m_state.entry.positionLy = playerLy;

        // Wherever the player is, that space is navigable (on-demand extension).
        m_state.navigationGrid.extendDomainTo({playerLy});
        {
            const auto index = m_state.navigationGrid.nearestIndexForPositionLy(playerLy, 0);
            auto& roots = m_state.overviewRoots;
            if (m_state.navigationGrid.isCellNavigable(index, 0) &&
                std::find(roots.begin(), roots.end(), index) == roots.end())
            {
                roots.push_back(index);
                ++m_state.hierarchyRevision;
            }
        }

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
            m_state.navigationFocusLy = playerLy;
            m_state.navigationFocusValid = true;
            m_state.selectedSystemId = entrySystemId;
            m_state.focusedSystemId = entrySystemId;

            // Opening the map shows the whole galaxy overview, the player's
            // root selected inside it.
            showGalaxyOverview(playerLy, false, 0.0);
        }

        m_state.entry.systemId = entrySystemId;
        m_state.entry.terminalCell = terminalCell;
        m_state.entry.valid = true;
    }

    void GalaxyMapView::resetNavigationToEntry()
    {
        cancelCameraFlight(false);

        m_state.navigationGrid.reset();
        m_state.camera = GalaxyCameraState {};

        m_state.navigationFocusLy =
            m_state.entry.positionLy;
        m_state.navigationFocusValid = true;
        m_state.selectedSystemId = m_state.entry.systemId;
        m_state.focusedSystemId = m_state.entry.systemId;

        // LEVEL 0: the galaxy overview, the player's root selected.
        showGalaxyOverview(m_state.entry.positionLy, false, 0.0);

        m_state.hoverVisualCell.reset();
        m_state.hoverVisualAlpha = 0.0f;
        m_state.hoverOutgoingCell.reset();
        m_state.hoverOutgoingAlpha = 0.0f;
        m_state.hoverVisualLastTimeSeconds = 0.0;
        resetTransientPointerState();
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

    GalaxyMapCameraSnapshot GalaxyMapView::cameraSnapshot(
        const Viewport& viewport
    ) const
    {
        GalaxyMapCameraSnapshot snapshot;
        snapshot.viewport = viewport;
        snapshot.basis = orbitCameraBasis(
            m_state.camera.yaw,
            m_state.camera.pitch
        );
        snapshot.distance =
            static_cast<double>(m_state.camera.distance);
        snapshot.fieldOfViewDeg = 48.0f;

        const glm::vec3 direction =
            glm::vec3(snapshot.basis.direction);

        const glm::vec3 target =
            m_state.camera.target;

        const glm::vec3 eye =
            target +
            direction * m_state.camera.distance;

        snapshot.target = glm::dvec3(target);
        snapshot.eye = glm::dvec3(eye);

        snapshot.view = glm::lookAt(
            eye,
            target,
            glm::vec3(snapshot.basis.up)
        );

        const float aspect =
            viewport.height > 0
                ? static_cast<float>(viewport.width) /
                    static_cast<float>(viewport.height)
                : 1.0f;

        snapshot.projection = glm::perspective(
            glm::radians(snapshot.fieldOfViewDeg),
            aspect,
            0.0001f,
            farPlane()
        );
        snapshot.mvp =
            snapshot.projection * snapshot.view;
        return snapshot;
    }

    glm::mat4 GalaxyMapView::viewMatrix() const
    {
        const OrbitCameraBasis basis =
            orbitCameraBasis(
                m_state.camera.yaw,
                m_state.camera.pitch
            );

        const glm::vec3 direction =
            glm::vec3(basis.direction);

        const glm::vec3 target =
            m_state.camera.target;

        const glm::vec3 eye =
            target +
            direction * m_state.camera.distance;

        return glm::lookAt(
            eye,
            target,
            glm::vec3(basis.up)
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
            farPlane()
        );
    }

    float GalaxyMapView::farPlane() const
    {
        // Far enough for the whole fitted overview behind its centre.
        return std::max(2000.0f, m_state.camera.distance * 3.0f);
    }

    glm::dvec3 GalaxyMapView::cameraDirectionWorld() const
    {
        return orbitCameraBasis(
            m_state.camera.yaw,
            m_state.camera.pitch
        ).direction;
    }

    glm::dvec3 GalaxyMapView::cameraUpWorld() const
    {
        return orbitCameraBasis(
            m_state.camera.yaw,
            m_state.camera.pitch
        ).up;
    }

    glm::dvec3 GalaxyMapView::cameraRightWorld() const
    {
        return orbitCameraBasis(
            m_state.camera.yaw,
            m_state.camera.pitch
        ).right;
    }

    glm::dvec3 GalaxyMapView::cameraEyeWorld() const
    {
        return
            glm::dvec3(m_state.camera.target) +
            cameraDirectionWorld() *
                static_cast<double>(m_state.camera.distance);
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

    GalaxyHierarchyBox GalaxyMapView::hierarchyBox() const
    {
        return galaxyHierarchyBox(
            m_state.navigationGrid,
            m_state.hierarchyParent,
            &m_state.overviewRoots
        );
    }

    float GalaxyMapView::cubeScreenRatioAt(
        const glm::dvec3& centerLy,
        double edgeLy,
        const glm::vec3& target,
        float distance,
        const Viewport& viewport,
        bool fitInside
    ) const
    {
        const OrbitCameraBasis basis =
            orbitCameraBasis(m_state.camera.yaw, m_state.camera.pitch);
        const glm::vec3 eye = target + glm::vec3(basis.direction) * distance;
        const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(basis.up));
        const glm::mat4 mvp = projectionMatrix(viewport) * view;

        float minX = 1.0e30f, minY = 1.0e30f, maxX = -1.0e30f, maxY = -1.0e30f;
        float largest = 0.0f;
        const double h = edgeLy * 0.5;
        for (int i = 0; i < 8; ++i)
        {
            const glm::dvec3 corner =
                centerLy + glm::dvec3((i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h);
            const glm::vec4 clip = mvp * glm::vec4(positionLyToRender(corner), 1.0f);
            // A corner behind the camera: the cube is around / beyond the
            // whole view.
            if (clip.w <= 1.0e-6f)
                return 1.0e9f;
            minX = std::min(minX, clip.x / clip.w);
            maxX = std::max(maxX, clip.x / clip.w);
            minY = std::min(minY, clip.y / clip.w);
            maxY = std::max(maxY, clip.y / clip.w);
            largest = std::max({largest, std::abs(clip.x / clip.w), std::abs(clip.y / clip.w)});
        }
        if (fitInside)
            return largest;
        // NDC spans 2 per axis.
        return std::max((maxX - minX) * 0.5f, (maxY - minY) * 0.5f);
    }

    float GalaxyMapView::cubeScreenRatio(
        const glm::dvec3& centerLy,
        double edgeLy,
        const Viewport& viewport
    ) const
    {
        return cubeScreenRatioAt(centerLy, edgeLy, m_state.camera.target, m_state.camera.distance, viewport);
    }

    float GalaxyMapView::fillDistanceAt(
        const glm::dvec3& centerLy,
        double edgeLy,
        const glm::vec3& target,
        const Viewport& viewport,
        float fill
    ) const
    {
        // The screen ratio falls monotonically with the distance: bisection
        // between "inside the cube" and its circumscribed-sphere fit.
        float inner = static_cast<float>(edgeLy) * RenderUnitsPerLightYear * 0.5f;
        float outer = std::max(
            inner * 2.0f,
            galaxyHierarchyFitDistance(edgeLy * RenderUnitsPerLightYear, glm::radians(48.0f),
                                       viewport.width, viewport.height, 1.5f));
        for (int k = 0; k < 40; ++k)
        {
            const float mid = 0.5f * (inner + outer);
            if (cubeScreenRatioAt(centerLy, edgeLy, target, mid, viewport, true) > fill)
                inner = mid;
            else
                outer = mid;
        }
        return outer;
    }

    float GalaxyMapView::hierarchyFitDistance(
        const Viewport& viewport
    ) const
    {
        // "The parent on the whole screen": its projected corners fill the
        // viewport in the limiting direction (current orbit angle).
        const GalaxyHierarchyBox box = hierarchyBox();
        return std::clamp(
            fillDistanceAt(box.centerLy, box.edgeLy, positionLyToRender(box.centerLy), viewport,
                           ParentFillRatio),
            m_controls.minDistance,
            m_controls.maxDistance
        );
    }

    void GalaxyMapView::cameraDistanceLimits(
        const Viewport& viewport,
        float& minimumDistance,
        float& maximumDistance
    ) const
    {
        /*
            Inside a parent the wheel goes deep enough for a child to fill
            the screen (it then becomes the parent). The overview never gets
            smaller than the screen; any other parent hands over to its own
            parent before (GalaxyMapInteraction wheel levels).
        */
        const GalaxyHierarchyBox box = hierarchyBox();
        const float fitted = hierarchyFitDistance(viewport);
        minimumDistance = std::max(m_controls.minDistance, fitted * m_controls.parentZoomInFraction);
        maximumDistance =
            box.parent.overview()
                ? std::min(m_controls.maxDistance,
                           fillDistanceAt(box.centerLy, box.edgeLy, m_state.camera.target, viewport,
                                          ParentFillRatio))
                : std::min(m_controls.maxDistance, fitted * m_controls.parentZoomOutFactor);
    }

    std::optional<navigation::GalaxyNavigationCell>
    GalaxyMapView::hierarchyChildAt(
        const glm::dvec3& positionLy
    ) const
    {
        return galaxyHierarchyChildAt(
            m_state.navigationGrid,
            hierarchyBox(),
            positionLy
        );
    }

    void GalaxyMapView::setHierarchyParent(
        const GalaxyHierarchyParent& parent
    )
    {
        m_state.hierarchyParent = parent;
        ++m_state.hierarchyRevision;

        auto& grid = m_state.navigationGrid;
        const GalaxyHierarchyBox box = hierarchyBox();

        /*
            The grid works on the parent's child level. Its anchor is the
            child holding the navigation focus, else the child nearest the
            parent centre (any navigable root for the sparse overview).
        */
        const glm::dvec3 anchorPoint =
            m_state.navigationFocusValid &&
                    galaxyHierarchyContains(box, m_state.navigationFocusLy)
                ? m_state.navigationFocusLy
                : box.centerLy;

        bool anchored = false;
        if (const auto child = galaxyHierarchyChildAt(grid, box, anchorPoint))
            anchored = grid.setAnchor(box.childLevel, child->index);

        if (!anchored && box.parent.overview())
        {
            double best = 1.0e300;
            navigation::GalaxyGridIndex bestIndex;
            for (const auto& root : grid.navigableRootCells())
            {
                const navigation::GalaxyGridIndex index {root[0], root[1], root[2]};
                const double d = glm::length(grid.cellCenterLy(index, 0) - anchorPoint);
                if (d < best)
                {
                    best = d;
                    bestIndex = index;
                }
            }
            anchored = best < 1.0e300 && grid.setAnchor(0, bestIndex);
        }

        if (!anchored)
        {
            grid.setAnchor(
                box.childLevel,
                grid.nearestIndexForPositionLy(box.centerLy, box.childLevel)
            );
        }

        grid.clearHoveredCell();
        m_state.hoverVisualCell.reset();
        m_state.hoverVisualAlpha = 0.0f;
        m_state.hoverOutgoingCell.reset();
        m_state.hoverOutgoingAlpha = 0.0f;
        m_state.hoverTarget = GalaxyPointerTarget {};
        m_state.hoverTargetValid = false;
        m_state.hoveredMediumObject = -1;
        m_state.cubeClickTracker.reset();
        m_state.starClickTracker.reset();
        m_state.mediumClickTracker.reset();
        m_state.hierarchyFaces.valid = false;
    }

    void GalaxyMapView::frameHierarchyParent(
        const Viewport& viewport,
        double nowSeconds,
        bool animate
    )
    {
        const GalaxyHierarchyBox box = hierarchyBox();
        const glm::vec3 target = positionLyToRender(box.centerLy);
        const float distance = hierarchyFitDistance(viewport);

        if (animate)
        {
            beginCameraFlight(target, distance, nowSeconds, true);
            return;
        }

        cancelCameraFlight(false);
        m_state.flightContextBox.reset();
        m_state.camera.target = target;
        m_state.camera.distance = distance;
    }

    bool GalaxyMapView::enterHierarchyCell(
        const navigation::GalaxyNavigationCell& cell,
        const Viewport& viewport,
        double nowSeconds,
        bool frameCamera
    )
    {
        auto& grid = m_state.navigationGrid;
        if (cell.level < 0 ||
            cell.level >= grid.maximumLevel() ||
            !grid.isCellNavigable(cell))
        {
            return false;
        }

        m_state.flightContextBox = hierarchyBox();

        GalaxyHierarchyParent parent;
        parent.level = cell.level;
        parent.index = cell.index;
        setHierarchyParent(parent);

        /*
            The selected object (star, cloud point) stays the navigation
            focus: its child inside the new parent is selected. Otherwise
            the entered cube itself stays the selected address.
        */
        const bool objectFocus =
            m_state.selectedSystemId >= 0 ||
            m_state.selectedMediumObject >= 0;
        if (objectFocus && m_state.navigationFocusValid)
        {
            if (const auto child = hierarchyChildAt(m_state.navigationFocusLy))
                grid.selectCell(*child);
        }
        else
        {
            grid.selectCell(grid.cell(cell.index, cell.level));
        }

        if (frameCamera)
            frameHierarchyParent(viewport, nowSeconds, true);
        else
            m_state.flightContextBox.reset();
        return true;
    }

    bool GalaxyMapView::ascendHierarchy(
        const Viewport& viewport,
        double nowSeconds
    )
    {
        return jumpToHierarchyLevel(
            m_state.hierarchyParent.level - 1,
            viewport,
            nowSeconds
        );
    }

    bool GalaxyMapView::jumpToHierarchyLevel(
        int level,
        const Viewport& viewport,
        double nowSeconds,
        bool frameCamera
    )
    {
        const GalaxyHierarchyParent current = m_state.hierarchyParent;
        if (current.overview() || level >= current.level || level < -1)
            return false;

        auto& grid = m_state.navigationGrid;
        const navigation::GalaxyNavigationCell currentCell =
            grid.cell(current.index, current.level);

        // The cube the user leaves stays visible and selected inside the
        // new parent: the ancestor of the old parent one level below it.
        const navigation::GalaxyNavigationCell cameFrom =
            grid.cell(
                grid.ancestorIndex(currentCell.index, currentCell.level, level + 1),
                level + 1
            );

        m_state.flightContextBox.reset();
        setHierarchyParent(galaxyHierarchyAncestor(grid, currentCell, level));
        grid.selectCell(cameFrom);

        if (frameCamera)
            frameHierarchyParent(viewport, nowSeconds, true);
        return true;
    }

    void GalaxyMapView::showGalaxyOverview(
        const glm::dvec3& selectRootAtLy,
        bool animate,
        double nowSeconds
    )
    {
        m_state.flightContextBox.reset();
        setHierarchyParent(GalaxyHierarchyParent {});

        auto& grid = m_state.navigationGrid;
        if (const auto root = hierarchyChildAt(selectRootAtLy))
            grid.selectCell(*root);
        else
            grid.selectCell(grid.anchorCell());

        frameHierarchyParent(m_state.lastViewport, nowSeconds, animate);
    }

    void GalaxyMapView::beginCameraFlight(
        const glm::vec3& destinationTarget,
        float destinationDistance,
        double nowSeconds,
        bool nestedFrameFlight
    )
    {
        destinationDistance =
            std::clamp(
                destinationDistance,
                m_controls.minDistance,
                m_controls.maxDistance
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

        // Entering / leaving a nested cube changes the scale x25: a
        // deliberate, readable flight (the child expands to the parent).
        const float duration =
            nestedFrameFlight
                ? m_visuals.cameraNestedFlightSeconds
                : m_visuals.cameraFlightMinSeconds +
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
            static_cast<double>(duration),
            nestedFrameFlight
        );
        m_state.nestedFrameFlight = nestedFrameFlight;
        if (!nestedFrameFlight || !m_state.cameraFlight.active())
            m_state.flightContextBox.reset();

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

        if (!m_state.cameraFlight.active())
            m_state.flightContextBox.reset();
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
            terminalCell.center
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
        m_state.selectedMediumObject = -1;

        // Keep the exact star position as the navigation target.
        focusHierarchyPoint(system->positionLy, animateCamera, nowSeconds);
        return true;
    }

    void GalaxyMapView::focusHierarchyPoint(
        const glm::dvec3& positionLy,
        bool animateCamera,
        double nowSeconds
    )
    {
        m_state.navigationFocusLy = positionLy;
        m_state.navigationFocusValid = true;

        /*
            A point outside the current parent moves the frame to the cube
            of the SAME level that contains it (the overview contains every
            navigable point); the working level is never changed here.
        */
        auto& grid = m_state.navigationGrid;
        if (!galaxyHierarchyContains(hierarchyBox(), positionLy))
        {
            // The overview frames the content roots only: a point outside
            // it is shown inside its own root.
            const int level = std::max(0, m_state.hierarchyParent.level);
            const auto index = grid.nearestIndexForPositionLy(positionLy, level);
            if (grid.isCellNavigable(index, level))
            {
                GalaxyHierarchyParent parent;
                parent.level = level;
                parent.index = index;
                m_state.flightContextBox.reset();
                setHierarchyParent(parent);
                frameHierarchyParent(m_state.lastViewport, nowSeconds, animateCamera);
            }
        }

        if (const auto child = hierarchyChildAt(positionLy))
            grid.selectCell(*child);
    }

}
