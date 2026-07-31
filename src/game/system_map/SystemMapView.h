#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <optional>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/types/Viewport.h"

#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/game/system_map/SystemMapVisualSettings.h"

namespace game::system_map
{
    /*
        Input and navigation tuning owned by the System map view.

        These values describe presentation and interaction only. They are
        deliberately separate from server simulation and celestial data.
    */
    struct SystemMapControlSettings
    {
        /*
            The outermost known body is normalized to this map radius.
            A newly opened system uses initialFitPadding around that radius,
            so the system is readable instead of appearing as a point.
        */
        float fittedSystemRadiusWorld = 70.0f;
        float initialFitPadding = 1.35f;

        /*
            Ring-and-cross markers are reserved for stars and planets.
            Sub-pixel moons use a compact filled point proxy instead; this
            preserves the satellite hierarchy without recreating reticle
            clutter around giant planets.
        */
        float proxyFadeStartRadiusRatio = 0.62f;

        float minPhysicalBodyRadiusPx = 0.70f;
        float starMarkerRadiusPx = 3.0f;
        float planetMarkerRadiusPx = 3.5f;
        float moonPointProxyRadiusPx = 2.25f;
        float moonPointProxyFadeEndRadiusPx = 2.75f;
        float pickMinBodyRadiusPx = 6.0f;

        float rotateSensitivity = 0.0055f;
        float pitchLimitRad = 1.32f;

        float cameraBodyAnchorMaxDistancePx = 28.0f;
        float rotationMaxStepRad = 0.10f;

        float zoomInFactor = 0.935f;
        float zoomOutFactor = 1.07f;
        float bodyZoomClearanceScale = 1.12f;
        double clickMoveThresholdPx = 5.0;

        float navigationCellInteractiveViewportFraction = 0.075f;
        float navigationCellInteractiveMinPx = 56.0f;
        float navigationCellPickRadiusPx = 18.0f;

        float navigationHoverFadeInSeconds = 0.18f;
        float navigationHoverFadeOutSeconds = 0.14f;

        /*
            At maximum zoom-out the single S0 root must not become smaller
            than this fraction of the shorter viewport side.
        */
        float navigationParentMinViewportFraction = 0.60f;

        double cubeDoubleClickMaxIntervalSeconds = 0.38;
        double cubeDoubleClickMaxDistancePx = 12.0;

        /*
            System navigation stops at S5 (about 2500 km).
            2.5 km/px keeps that terminal cube inspectable at roughly
            1000 px. Smaller scales belong to Details.
        */
        double minKmPerPixel = 2.5;

        float pickMaxBodyRadiusPx = 8000.0f;
        float pickHaloBasePx = 48.0f;
        float pickHaloRadiusFactor = 0.08f;
        float pickHaloMaxPx = 220.0f;
        float pickScoreDiskWeight = 10000.0f;

        int sparsePivotMaxVisibleBodies = 2;
        float sparsePivotViewportPaddingPx = 96.0f;
    };

    /*
        Mutable state belonging exclusively to the System map mode.

        Input is coordinated by SystemMapInteraction and draw orchestration
        by SystemMapSceneRenderer. SystemMapRenderer owns only shared rendering
        resources and the frame-local pick cache.
    */
    struct SystemMapViewState
    {
        SystemCameraFlightState cameraFlight;
        SystemCameraState camera;

        navigation::SystemNavigationGrid navigationGrid;

        std::optional<navigation::CubicNavigationCell>
            hoverVisualCell;

        float hoverVisualAlpha = 0.0f;

        std::optional<navigation::CubicNavigationCell>
            hoverOutgoingCell;

        float hoverOutgoingAlpha = 0.0f;
        double hoverVisualLastTimeSeconds = 0.0;

        navigation::CubicNavigationClickTracker<
                navigation::CubicGridIndex
            > cubeClickTracker;

        int lastCameraFitSystemId = -1;

        std::string selectedBodyId;
        std::string selectedHubId;
        std::string selectedHubParentBodyId;

        /*
            CubicNavigationGrid owns a selected root cell immediately after
            reset so it can render a stable navigation frame. That implicit
            selection is not a user command and must not enable Details.
        */
        bool navigationCellExplicitlySelected = false;

        float lastScale = 1.0f;

        int presentationSystemId = -1;
        double presentationSourceTimeSeconds = 0.0;
        double presentationWallTimeSeconds = 0.0;
        double presentationTimeScale = 1.0;

        glm::dvec3 orbitPivotAbsolute {0.0, 0.0, 0.0};
        bool orbitPivotActive = false;
    };

    class SystemMapView
    {
    public:
        /*
            The camera distance keeps the historical "visible half-height"
            semantic even though the map uses a perspective projection.
        */
        static constexpr float minimumCameraHalfHeight =
            0.0000005f;

        static constexpr float maximumCameraHalfHeight =
            5000.0f;

        SystemMapViewState& state() noexcept
        {
            return m_state;
        }

        const SystemMapViewState& state() const noexcept
        {
            return m_state;
        }

        SystemMapControlSettings& controls() noexcept
        {
            return m_controls;
        }

        const SystemMapControlSettings& controls() const noexcept
        {
            return m_controls;
        }

        SystemMapVisualSettings& visuals() noexcept
        {
            return m_visuals;
        }

        const SystemMapVisualSettings& visuals() const noexcept
        {
            return m_visuals;
        }

        glm::mat4 viewMatrix() const
        {
            const glm::dvec3 direction =
                cameraDirectionWorld();

            const glm::dvec3 up =
                cameraUpWorld();

            /*
                All System-map render positions are camera-target relative,
                so the perspective camera always looks at local zero.
            */
            const glm::dvec3 eye =
                direction *
                cameraEyeDistance();

            return glm::lookAt(
                glm::vec3(eye),
                glm::vec3(0.0f),
                glm::vec3(up)
            );
        }

        glm::mat4 projectionMatrix(
            const Viewport& viewport
        ) const
        {
            const float aspect =
                viewport.height > 0
                    ? static_cast<float>(viewport.width) /
                        static_cast<float>(viewport.height)
                    : 1.0f;

            const float halfHeight =
                std::clamp(
                    m_state.camera.distance,
                    minimumCameraHalfHeight,
                    maximumCameraHalfHeight
                );

            return glm::perspective(
                glm::radians(
                    m_visuals.projectionFieldOfViewDeg
                ),
                aspect,
                nearPlane(halfHeight),
                farPlane(
                    halfHeight,
                    m_visuals.projectionFieldOfViewDeg
                )
            );
        }

        glm::dvec3 cameraDirectionWorld() const
        {
            return glm::dvec3(
                cameraDirectionFromYawPitch(
                    m_state.camera.yaw,
                    m_state.camera.pitch
                )
            );
        }

        glm::dvec3 cameraUpWorld() const
        {
            return glm::dvec3(
                cameraUpFromYawPitch(
                    m_state.camera.yaw,
                    m_state.camera.pitch
                )
            );
        }

        glm::dvec3 cameraRightWorld() const
        {
            const glm::dvec3 right =
                glm::cross(
                    cameraUpWorld(),
                    cameraDirectionWorld()
                );

            const double length =
                glm::length(right);

            if (length <= 0.000000001)
                return glm::dvec3(1.0, 0.0, 0.0);

            return right / length;
        }

        double cameraEyeDistance() const
        {
            return static_cast<double>(
                perspectiveEyeDistance(
                    m_state.camera.distance,
                    m_visuals.projectionFieldOfViewDeg
                )
            );
        }

        glm::dvec3 cameraEyeAbsolute() const
        {
            return
                m_state.camera.target +
                cameraDirectionWorld() *
                    cameraEyeDistance();
        }

        double targetPlaneWorldUnitsPerPixel(
            const Viewport& viewport
        ) const
        {
            return
                static_cast<double>(
                    std::clamp(
                        m_state.camera.distance,
                        minimumCameraHalfHeight,
                        maximumCameraHalfHeight
                    )
                ) *
                2.0 /
                static_cast<double>(
                    std::max(viewport.height, 1)
                );
        }

        glm::vec2 projectAbsoluteToScreen(
            const Viewport& viewport,
            const glm::dvec3& absolutePosition,
            bool& visible,
            float& depth
        ) const
        {
            const glm::dvec3 relative =
                absolutePosition -
                m_state.camera.target;

            const glm::vec4 clip =
                projectionMatrix(viewport) *
                viewMatrix() *
                glm::vec4(
                    glm::vec3(relative),
                    1.0f
                );

            visible = false;
            depth = 2.0f;

            if (clip.w <= 0.00001f)
                return glm::vec2(0.0f);

            const glm::vec3 ndc =
                glm::vec3(clip) /
                clip.w;

            visible =
                ndc.x >= -1.0f && ndc.x <= 1.0f &&
                ndc.y >= -1.0f && ndc.y <= 1.0f &&
                ndc.z >= -1.0f && ndc.z <= 1.0f;

            depth = ndc.z;

            return glm::vec2(
                (ndc.x * 0.5f + 0.5f) *
                    static_cast<float>(viewport.width),
                (1.0f - (ndc.y * 0.5f + 0.5f)) *
                    static_cast<float>(viewport.height)
            );
        }

        glm::dvec3 targetPlanePointFromScreen(
            const Viewport& viewport,
            double localMouseX,
            double localMouseY
        ) const
        {
            const double safeWidth =
                static_cast<double>(
                    std::max(viewport.width, 1)
                );

            const double safeHeight =
                static_cast<double>(
                    std::max(viewport.height, 1)
                );

            const double aspect =
                safeWidth / safeHeight;

            const double halfHeight =
                static_cast<double>(
                    std::clamp(
                        m_state.camera.distance,
                        minimumCameraHalfHeight,
                        maximumCameraHalfHeight
                    )
                );

            const double ndcX =
                localMouseX / safeWidth * 2.0 - 1.0;

            const double ndcY =
                1.0 - localMouseY / safeHeight * 2.0;

            return
                m_state.camera.target +
                cameraRightWorld() *
                    ndcX * halfHeight * aspect +
                cameraUpWorld() *
                    ndcY * halfHeight;
        }

        void panCameraByScreenDelta(
            const Viewport& viewport,
            double deltaX,
            double deltaY
        )
        {
            const double worldUnitsPerPixel =
                targetPlaneWorldUnitsPerPixel(
                    viewport
                );

            m_state.camera.target -=
                cameraRightWorld() *
                deltaX *
                worldUnitsPerPixel;

            m_state.camera.target +=
                cameraUpWorld() *
                deltaY *
                worldUnitsPerPixel;
        }

        void orbitCameraAroundPivot(
            const glm::dvec3& pivotAbsolute,
            float yawDelta,
            float pitchDelta,
            float pitchLimitRad
        )
        {
            const float oldYaw =
                m_state.camera.yaw;

            const float oldPitch =
                m_state.camera.pitch;

            const float newYaw =
                wrapAngleRad(
                    oldYaw + yawDelta
                );

            const float newPitch =
                std::clamp(
                    oldPitch + pitchDelta,
                    -pitchLimitRad,
                    pitchLimitRad
                );

            const glm::dvec3 oldDirection(
                cameraDirectionFromYawPitch(
                    oldYaw,
                    oldPitch
                )
            );

            const glm::dvec3 oldUp(
                cameraUpFromYawPitch(
                    oldYaw,
                    oldPitch
                )
            );

            const glm::dvec3 oldRight =
                normalizedCameraRight(
                    oldUp,
                    oldDirection
                );

            const glm::dvec3 newDirection(
                cameraDirectionFromYawPitch(
                    newYaw,
                    newPitch
                )
            );

            const glm::dvec3 newUp(
                cameraUpFromYawPitch(
                    newYaw,
                    newPitch
                )
            );

            const glm::dvec3 newRight =
                normalizedCameraRight(
                    newUp,
                    newDirection
                );

            const glm::dmat3 oldBasis(
                oldRight,
                oldUp,
                oldDirection
            );

            const glm::dmat3 newBasis(
                newRight,
                newUp,
                newDirection
            );

            const glm::dmat3 worldRotation =
                newBasis *
                glm::transpose(oldBasis);

            m_state.camera.target =
                pivotAbsolute +
                worldRotation *
                    (
                        m_state.camera.target -
                        pivotAbsolute
                    );

            m_state.camera.yaw =
                newYaw;

            m_state.camera.pitch =
                newPitch;
        }

        void zoomCameraAroundPivot(
            const glm::dvec3& pivotAbsolute,
            float zoomFactor,
            float minimumDistance,
            float maximumDistance,
            double minimumEyeDistanceFromPivot = 0.0
        )
        {
            minimumDistance =
                std::clamp(
                    minimumDistance,
                    minimumCameraHalfHeight,
                    maximumCameraHalfHeight
                );

            maximumDistance =
                std::clamp(
                    maximumDistance,
                    minimumDistance,
                    maximumCameraHalfHeight
                );

            const float oldDistance =
                std::clamp(
                    m_state.camera.distance,
                    minimumDistance,
                    maximumDistance
                );

            const float requestedDistance =
                std::clamp(
                    oldDistance * zoomFactor,
                    minimumDistance,
                    maximumDistance
                );

            double poseScale =
                static_cast<double>(requestedDistance) /
                static_cast<double>(
                    std::max(
                        oldDistance,
                        minimumCameraHalfHeight
                    )
                );

            /*
                A body pivot uses distance above its safe surface rather than
                distance to its centre. Zoom-in therefore approaches the
                surface asymptotically and cannot jump through the body.

                Zoom-out keeps the ordinary pose scale so a camera recovered
                from legacy inside-body state moves outward immediately.
            */
            if (minimumEyeDistanceFromPivot > 0.0 &&
                poseScale < 1.0)
            {
                const double currentEyeDistance =
                    glm::length(
                        cameraEyeAbsolute() -
                        pivotAbsolute
                    );

                const double clearanceEpsilon =
                    std::max(
                        0.000000001,
                        minimumEyeDistanceFromPivot * 0.000001
                    );

                if (currentEyeDistance <=
                    minimumEyeDistanceFromPivot +
                        clearanceEpsilon)
                {
                    return;
                }

                const double currentClearance =
                    currentEyeDistance -
                    minimumEyeDistanceFromPivot;

                const double requestedEyeDistance =
                    minimumEyeDistanceFromPivot +
                    currentClearance * poseScale;

                poseScale =
                    requestedEyeDistance /
                    currentEyeDistance;
            }

            float newDistance =
                std::clamp(
                    static_cast<float>(
                        static_cast<double>(oldDistance) *
                        poseScale
                    ),
                    minimumDistance,
                    maximumDistance
                );

            poseScale =
                static_cast<double>(newDistance) /
                static_cast<double>(
                    std::max(
                        oldDistance,
                        minimumCameraHalfHeight
                    )
                );

            m_state.camera.target =
                pivotAbsolute +
                (
                    m_state.camera.target -
                    pivotAbsolute
                ) *
                poseScale;

            m_state.camera.distance =
                newDistance;
        }

        void beginCameraFlight(
            const glm::dvec3& destinationTarget,
            float destinationDistance,
            double nowSeconds
        )
        {
            destinationDistance =
                std::clamp(
                    destinationDistance,
                    minimumCameraHalfHeight,
                    maximumCameraHalfHeight
                );

            m_state.cameraFlight.startTarget =
                m_state.camera.target;

            m_state.cameraFlight.destinationTarget =
                destinationTarget;

            m_state.cameraFlight.startDistance =
                m_state.camera.distance;

            m_state.cameraFlight.destinationDistance =
                destinationDistance;

            m_state.cameraFlight.startTimeSeconds =
                nowSeconds;

            m_state.cameraFlight.durationSeconds =
                0.58;

            m_state.cameraFlight.active =
                true;

            m_state.camera.rotating = false;
            m_state.camera.panning = false;
            m_state.orbitPivotActive = false;
        }

        void updateCameraFlight(
            double nowSeconds
        )
        {
            if (!m_state.cameraFlight.active)
                return;

            const double elapsed =
                std::max(
                    0.0,
                    nowSeconds -
                        m_state.cameraFlight.startTimeSeconds
                );

            const float linearProgress =
                static_cast<float>(
                    std::clamp(
                        elapsed /
                            m_state.cameraFlight.durationSeconds,
                        0.0,
                        1.0
                    )
                );

            const float progress =
                linearProgress *
                linearProgress *
                linearProgress *
                (
                    linearProgress *
                    (
                        linearProgress * 6.0f -
                        15.0f
                    ) +
                    10.0f
                );

            m_state.camera.target =
                glm::mix(
                    m_state.cameraFlight.startTarget,
                    m_state.cameraFlight.destinationTarget,
                    static_cast<double>(progress)
                );

            const float startDistance =
                std::max(
                    m_state.cameraFlight.startDistance,
                    0.0000001f
                );

            const float destinationDistance =
                std::max(
                    m_state.cameraFlight.destinationDistance,
                    0.0000001f
                );

            m_state.camera.distance =
                std::exp(
                    std::log(startDistance) +
                    (
                        std::log(destinationDistance) -
                        std::log(startDistance)
                    ) *
                    progress
                );

            if (linearProgress >= 1.0f)
            {
                m_state.camera.target =
                    m_state.cameraFlight.destinationTarget;

                m_state.camera.distance =
                    m_state.cameraFlight.destinationDistance;

                m_state.cameraFlight.active = false;
            }
        }

        void cancelCameraFlight(
            bool snapToDestination
        )
        {
            if (!m_state.cameraFlight.active)
                return;

            if (snapToDestination)
            {
                m_state.camera.target =
                    m_state.cameraFlight.destinationTarget;

                m_state.camera.distance =
                    m_state.cameraFlight.destinationDistance;
            }

            m_state.cameraFlight.active = false;
        }

        float navigationAnchorDiameterPx(
            const Viewport& viewport
        ) const
        {
            if (!m_state.navigationGrid.enabled() ||
                m_state.lastScale <= 0.0f ||
                viewport.height <= 0)
            {
                return 0.0f;
            }

            const auto cell =
                m_state.navigationGrid.anchorCell();

            const float cellEdgeWorld =
                static_cast<float>(
                    cell.size *
                    static_cast<double>(m_state.lastScale)
                );

            return
                navigation::
                    cubicNavigationOrthographicProjectedDiameterPx(
                        cellEdgeWorld,
                        m_state.camera.distance,
                        viewport.height
                    );
        }

        bool navigationCellsInteractive(
            const Viewport& viewport
        ) const
        {
            if (!m_state.navigationGrid.enabled() ||
                m_state.lastScale <= 0.0f ||
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

        glm::dvec3 navigationCursorAu() const
        {
            if (m_state.lastScale <= 0.0f)
                return glm::dvec3(0.0);

            return
                m_state.camera.target /
                static_cast<double>(m_state.lastScale);
        }

        void syncNavigationAnchorToCursor()
        {
            if (!m_state.navigationGrid.enabled() ||
                m_state.lastScale <= 0.0f)
            {
                return;
            }

            const int minimumLevel =
                m_state.navigationGrid
                    .definition()
                    .minimumLevel;

            const glm::dvec3 cursorAu =
                navigationCursorAu();

            const navigation::CubicGridIndex
                systemRootIndex {};

            if (m_state.navigationGrid.nearestIndexForPosition(
                    cursorAu,
                    minimumLevel
                ) != systemRootIndex)
            {
                return;
            }

            const auto candidateIndex =
                m_state.navigationGrid.nearestIndexForPosition(
                    cursorAu,
                    m_state.navigationGrid.level()
                );

            if (candidateIndex ==
                m_state.navigationGrid.anchorIndex())
            {
                return;
            }

            m_state.navigationGrid.setAnchorIndex(
                candidateIndex
            );

            m_state.navigationGrid.clearHoveredCell();

            m_state.hoverVisualCell.reset();
            m_state.hoverVisualAlpha = 0.0f;

            m_state.hoverOutgoingCell.reset();
            m_state.hoverOutgoingAlpha = 0.0f;

            m_state.cubeClickTracker.reset();
        }

        std::optional<navigation::CubicNavigationCell>
        resolvedTerminalSelection() const
        {
            if (!m_state.navigationGrid.enabled() ||
                !m_state.navigationGrid.hasSelectedCell() ||
                !m_state.navigationCellExplicitlySelected)
            {
                return std::nullopt;
            }

            const auto selected =
                m_state.navigationGrid.selectedCell();

            const int maximumLevel =
                m_state.navigationGrid
                    .definition()
                    .maximumLevel;

            navigation::CubicGridIndex terminalIndex =
                selected.index;

            int terminalLevel =
                selected.level;

            const std::int64_t subdivision =
                static_cast<std::int64_t>(
                    m_state.navigationGrid.subdivision()
                );

            /*
                Every selected cube has one unambiguous central descendant.
                Details may therefore open immediately from S0/S1/... while
                still receiving the same terminal-volume address as a manual
                descent through the centre at every remaining level.
            */
            while (terminalLevel < maximumLevel)
            {
                terminalIndex.x *= subdivision;
                terminalIndex.y *= subdivision;
                terminalIndex.z *= subdivision;
                ++terminalLevel;
            }

            return m_state.navigationGrid.cell(
                terminalIndex,
                terminalLevel
            );
        }

        float navigationMaximumCameraDistance(
            const Viewport& viewport
        ) const
        {
            if (!m_state.navigationGrid.enabled() ||
                m_state.lastScale <= 0.0f ||
                viewport.width <= 0 ||
                viewport.height <= 0)
            {
                return maximumCameraHalfHeight;
            }

            const int boundaryLevel =
                m_state.navigationGrid
                    .definition()
                    .minimumLevel;

            const double boundaryEdgeWorld =
                m_state.navigationGrid.cellSize(boundaryLevel) *
                static_cast<double>(m_state.lastScale);

            const double viewportReferencePx =
                static_cast<double>(
                    std::max(
                        1,
                        std::min(
                            viewport.width,
                            viewport.height
                        )
                    )
                );

            const double minimumParentDiameterPx =
                viewportReferencePx *
                static_cast<double>(
                    m_controls
                        .navigationParentMinViewportFraction
                );

            const double maximumHalfHeight =
                boundaryEdgeWorld *
                1.35 *
                static_cast<double>(
                    std::max(viewport.height, 1)
                ) /
                (
                    2.0 *
                    std::max(
                        minimumParentDiameterPx,
                        1.0
                    )
                );

            return std::clamp(
                static_cast<float>(maximumHalfHeight),
                minimumCameraHalfHeight,
                maximumCameraHalfHeight
            );
        }

        glm::dvec3 navigationBoundaryCenterWorld() const
        {
            if (!m_state.navigationGrid.enabled() ||
                m_state.lastScale <= 0.0f)
            {
                return m_state.camera.target;
            }

            const int boundaryLevel =
                m_state.navigationGrid
                    .definition()
                    .minimumLevel;

            const navigation::CubicGridIndex
                boundaryIndex {};

            const auto boundaryCell =
                m_state.navigationGrid.cell(
                    boundaryIndex,
                    boundaryLevel
                );

            return
                boundaryCell.center *
                static_cast<double>(m_state.lastScale);
        }

        void constrainCameraToNavigationBoundary(
            const Viewport& viewport
        )
        {
            if (!m_state.navigationGrid.enabled() ||
                m_state.lastScale <= 0.0f ||
                viewport.width <= 0 ||
                viewport.height <= 0)
            {
                return;
            }

            const float maximumDistance =
                navigationMaximumCameraDistance(viewport);

            const float boundaryEpsilon =
                std::max(
                    0.000001f,
                    maximumDistance * 0.0005f
                );

            const int boundaryLevel =
                m_state.navigationGrid
                    .definition()
                    .minimumLevel;

            const navigation::CubicGridIndex
                boundaryIndex {};

            const auto boundaryCell =
                m_state.navigationGrid.cell(
                    boundaryIndex,
                    boundaryLevel
                );

            const auto& frame =
                m_state.navigationGrid.frame();

            const glm::dvec3 axisX =
                glm::normalize(frame.axisX);

            const glm::dvec3 axisY =
                glm::normalize(frame.axisY);

            const glm::dvec3 axisZ =
                glm::normalize(frame.axisZ);

            const double renderScale =
                static_cast<double>(m_state.lastScale);

            const glm::dvec3 boundaryCenter =
                boundaryCell.center *
                renderScale;

            const navigation::CubicGridIndex
                systemRootIndex {};

            const auto rootCell =
                m_state.navigationGrid.cell(
                    systemRootIndex,
                    m_state.navigationGrid
                        .definition()
                        .minimumLevel
                );

            const glm::dvec3 rootCenter =
                rootCell.center *
                renderScale;

            const double rootHalfEdge =
                rootCell.size *
                renderScale *
                0.5;

            const auto clampTarget =
                [&](const glm::dvec3& requestedTarget,
                    float requestedDistance)
                {
                    const float safeDistance =
                        std::clamp(
                            requestedDistance,
                            minimumCameraHalfHeight,
                            maximumDistance
                        );

                    if (safeDistance >=
                        maximumDistance -
                            boundaryEpsilon)
                    {
                        return boundaryCenter;
                    }

                    const double linearFreedom =
                        std::clamp(
                            1.0 -
                                static_cast<double>(safeDistance) /
                                static_cast<double>(
                                    std::max(
                                        maximumDistance,
                                        0.000001f
                                    )
                                ),
                            0.0,
                            1.0
                        );

                    const double smoothFreedom =
                        linearFreedom *
                        linearFreedom *
                        (
                            3.0 -
                            2.0 * linearFreedom
                        );

                    const glm::dvec3 relative =
                        requestedTarget -
                        boundaryCenter;

                    const glm::dvec3 rootRelative =
                        rootCenter -
                        boundaryCenter;

                    const auto clampAxis =
                        [&](const glm::dvec3& axis)
                        {
                            const double rootCenterOnAxis =
                                glm::dot(
                                    rootRelative,
                                    axis
                                );

                            const double minimumOffset =
                                (
                                    rootCenterOnAxis -
                                    rootHalfEdge
                                ) *
                                smoothFreedom;

                            const double maximumOffset =
                                (
                                    rootCenterOnAxis +
                                    rootHalfEdge
                                ) *
                                smoothFreedom;

                            return std::clamp(
                                glm::dot(relative, axis),
                                minimumOffset,
                                maximumOffset
                            );
                        };

                    const double localX =
                        clampAxis(axisX);

                    const double localY =
                        clampAxis(axisY);

                    const double localZ =
                        clampAxis(axisZ);

                    return
                        boundaryCenter +
                        axisX * localX +
                        axisY * localY +
                        axisZ * localZ;
                };

            m_state.camera.distance =
                std::min(
                    m_state.camera.distance,
                    maximumDistance
                );

            m_state.camera.target =
                clampTarget(
                    m_state.camera.target,
                    m_state.camera.distance
                );

            if (m_state.cameraFlight.active)
            {
                m_state.cameraFlight.destinationDistance =
                    std::min(
                        m_state.cameraFlight.destinationDistance,
                        maximumDistance
                    );

                m_state.cameraFlight.destinationTarget =
                    clampTarget(
                        m_state.cameraFlight.destinationTarget,
                        m_state.cameraFlight.destinationDistance
                    );
            }
        }

    private:
        static float wrapAngleRad(
            float angle
        )
        {
            const float twoPi =
                6.28318530717958647692f;

            while (angle > 3.14159265358979323846f)
                angle -= twoPi;

            while (angle < -3.14159265358979323846f)
                angle += twoPi;

            return angle;
        }

        static glm::dvec3 normalizedCameraRight(
            const glm::dvec3& up,
            const glm::dvec3& direction
        )
        {
            const glm::dvec3 right =
                glm::cross(
                    up,
                    direction
                );

            const double length =
                glm::length(right);

            if (length <= 0.000000001)
                return glm::dvec3(1.0, 0.0, 0.0);

            return right / length;
        }

        static constexpr float depthMultiplier =
            8.0f;

        static float depthHalfRange(
            float cameraHalfHeight
        )
        {
            const float safeHalfHeight =
                std::clamp(
                    cameraHalfHeight,
                    minimumCameraHalfHeight,
                    maximumCameraHalfHeight
                );

            return std::max(
                safeHalfHeight * depthMultiplier,
                0.000001f
            );
        }

        static float nearPlane(
            float cameraHalfHeight
        )
        {
            return std::max(
                depthHalfRange(cameraHalfHeight) *
                    0.0001f,
                0.000000001f
            );
        }

        static float perspectiveEyeDistance(
            float cameraHalfHeight,
            float fieldOfViewDeg
        )
        {
            const float safeHalfHeight =
                std::clamp(
                    cameraHalfHeight,
                    minimumCameraHalfHeight,
                    maximumCameraHalfHeight
                );

            const float halfFovRad =
                glm::radians(
                    fieldOfViewDeg * 0.5f
                );

            return
                safeHalfHeight /
                std::max(
                    std::tan(halfFovRad),
                    0.0001f
                );
        }

        static float farPlane(
            float cameraHalfHeight,
            float fieldOfViewDeg
        )
        {
            return
                perspectiveEyeDistance(
                    cameraHalfHeight,
                    fieldOfViewDeg
                ) +
                depthHalfRange(cameraHalfHeight) *
                    2.0f;
        }

        static glm::vec3 cameraDirectionFromYawPitch(
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

        static glm::vec3 cameraUpFromYawPitch(
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

    private:
        SystemMapViewState m_state;
        SystemMapControlSettings m_controls;
        SystemMapVisualSettings m_visuals;
    };
}
