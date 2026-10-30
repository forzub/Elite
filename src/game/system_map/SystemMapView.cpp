#include "src/game/system_map/SystemMapView.h"
#include "src/game/system_map/HierarchyFramePainter.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace game::system_map
{
SystemMapViewState& SystemMapView::state() noexcept
{
    return m_state;
}

const SystemMapViewState& SystemMapView::state() const noexcept
{
    return m_state;
}

SystemMapControlSettings& SystemMapView::controls() noexcept
{
    return m_controls;
}

const SystemMapControlSettings& SystemMapView::controls() const noexcept
{
    return m_controls;
}

SystemMapVisualSettings& SystemMapView::visuals() noexcept
{
    return m_visuals;
}

const SystemMapVisualSettings& SystemMapView::visuals() const noexcept
{
    return m_visuals;
}

void SystemMapView::reset()
{
    m_state.camera = SystemCameraState {};
    m_state.cameraFlight = SystemCameraFlightState {};

    m_state.hoverVisualCell.reset();
    m_state.hoverVisualAlpha = 0.0f;
    m_state.hoverOutgoingCell.reset();
    m_state.hoverOutgoingAlpha = 0.0f;
    m_state.hoverVisualLastTimeSeconds = 0.0;
    m_state.cubeClickTracker.reset();
    m_state.lastCameraFitSystemId = -1;

    m_state.selectedBodyId.clear();
    m_state.selectedHubId.clear();
    m_state.selectedHubParentBodyId.clear();
    m_state.navigationCellExplicitlySelected = false;

    m_state.orbitPivotAbsolute = glm::dvec3(0.0);
    m_state.orbitPivotActive = false;

    m_state.hierarchyParent = GalaxyHierarchyParent {0, {}};
    ++m_state.hierarchyRevision;
    m_state.hierarchyFaces = GalaxyHierarchyFaces {};
    m_state.hoverLayerAxis = -1;
    m_state.pointerGesture.reset();
    m_state.pressTarget = SystemPointerTarget {};
    m_state.hoverTarget = SystemPointerTarget {};
    m_state.hoverTargetValid = false;
    m_state.bodyClickTracker.reset();
    m_state.presentedCubeMarkers.clear();
}

void SystemMapView::resetNavigationToLevelZero(
    const Viewport& viewport
)
{
    if (!m_state.navigationGrid.enabled())
        return;

    cancelCameraFlight(false);

    m_state.navigationGrid.reset();
    m_state.navigationGrid.setAnchorFromPosition(
        glm::dvec3(0.0)
    );
    m_state.navigationGrid.selectCell(
        m_state.navigationGrid.anchorCell()
    );
    m_state.navigationCellExplicitlySelected = false;
    m_state.navigationGrid.clearHoveredCell();

    m_state.camera = SystemCameraState {};

    const float defaultDistance =
        m_controls.fittedSystemRadiusWorld *
        m_controls.initialFitPadding;

    const float maximumDistance =
        std::max(
            minimumCameraHalfHeight,
            navigationMaximumCameraDistance(viewport)
        );

    m_state.camera.distance =
        std::clamp(
            defaultDistance,
            minimumCameraHalfHeight,
            maximumDistance
        );

    m_state.cameraFlight = SystemCameraFlightState {};

    m_state.hoverVisualCell.reset();
    m_state.hoverVisualAlpha = 0.0f;
    m_state.hoverOutgoingCell.reset();
    m_state.hoverOutgoingAlpha = 0.0f;
    m_state.hoverVisualLastTimeSeconds = 0.0;
    m_state.cubeClickTracker.reset();
    m_state.orbitPivotActive = false;

    // LEVEL 0: the S0 root as the parent, on the whole screen.
    resetHierarchyToRoot(viewport);
    m_state.navigationGrid.selectCell(m_state.navigationGrid.cell({}, 0));
    m_state.navigationCellExplicitlySelected = false;
}

void SystemMapView::suppressCameraGesture(
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
}

SystemMapCameraSnapshot SystemMapView::cameraSnapshot(
    const Viewport& viewport
) const
{
    SystemMapCameraSnapshot snapshot;
    snapshot.viewport = viewport;
    snapshot.targetAbsolute = m_state.camera.target;
    snapshot.basis = orbitCameraBasis(
        m_state.camera.yaw,
        m_state.camera.pitch
    );
    snapshot.halfHeight =
        static_cast<double>(
            std::clamp(
                m_state.camera.distance,
                minimumCameraHalfHeight,
                maximumCameraHalfHeight
            )
        );
    snapshot.eyeDistance =
        static_cast<double>(
            perspectiveEyeDistance(
                static_cast<float>(snapshot.halfHeight),
                m_visuals.projectionFieldOfViewDeg
            )
        );
    snapshot.eyeAbsolute =
        snapshot.targetAbsolute +
        snapshot.basis.direction *
            snapshot.eyeDistance;
    snapshot.worldUnitsPerPixel =
        snapshot.halfHeight * 2.0 /
        static_cast<double>(
            std::max(viewport.height, 1)
        );

    const glm::dvec3 relativeEye =
        snapshot.basis.direction *
        snapshot.eyeDistance;

    snapshot.view = glm::lookAt(
        glm::vec3(relativeEye),
        glm::vec3(0.0f),
        glm::vec3(snapshot.basis.up)
    );

    const float aspect =
        viewport.height > 0
            ? static_cast<float>(viewport.width) /
                static_cast<float>(viewport.height)
            : 1.0f;

    snapshot.projection = glm::perspective(
        glm::radians(
            m_visuals.projectionFieldOfViewDeg
        ),
        aspect,
        nearPlane(
            static_cast<float>(snapshot.halfHeight)
        ),
        farPlane(
            static_cast<float>(snapshot.halfHeight),
            m_visuals.projectionFieldOfViewDeg
        )
    );
    snapshot.mvp =
        snapshot.projection * snapshot.view;
    return snapshot;
}

glm::mat4 SystemMapView::viewMatrix() const
{
    const OrbitCameraBasis basis =
        orbitCameraBasis(
            m_state.camera.yaw,
            m_state.camera.pitch
        );

    const glm::dvec3 eye =
        basis.direction *
        cameraEyeDistance();

    return glm::lookAt(
        glm::vec3(eye),
        glm::vec3(0.0f),
        glm::vec3(basis.up)
    );
}

glm::mat4 SystemMapView::projectionMatrix(
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

glm::dvec3 SystemMapView::cameraDirectionWorld() const
{
    return orbitCameraBasis(
        m_state.camera.yaw,
        m_state.camera.pitch
    ).direction;
}

glm::dvec3 SystemMapView::cameraUpWorld() const
{
    return orbitCameraBasis(
        m_state.camera.yaw,
        m_state.camera.pitch
    ).up;
}

glm::dvec3 SystemMapView::cameraRightWorld() const
{
    return orbitCameraBasis(
        m_state.camera.yaw,
        m_state.camera.pitch
    ).right;
}

double SystemMapView::cameraEyeDistance() const
{
    return static_cast<double>(
        perspectiveEyeDistance(
            m_state.camera.distance,
            m_visuals.projectionFieldOfViewDeg
        )
    );
}

glm::dvec3 SystemMapView::cameraEyeAbsolute() const
{
    return
        m_state.camera.target +
        cameraDirectionWorld() *
            cameraEyeDistance();
}

double SystemMapView::targetPlaneWorldUnitsPerPixel(
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

glm::vec2 SystemMapView::projectAbsoluteToScreen(
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

glm::dvec3 SystemMapView::targetPlanePointFromScreen(
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

void SystemMapView::panCameraByScreenDelta(
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

void SystemMapView::orbitCameraAroundPivot(
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

    const OrbitCameraBasis oldCameraBasis =
        orbitCameraBasis(oldYaw, oldPitch);

    const OrbitCameraBasis newCameraBasis =
        orbitCameraBasis(newYaw, newPitch);

    const glm::dvec3& oldDirection =
        oldCameraBasis.direction;

    const glm::dvec3& oldUp =
        oldCameraBasis.up;

    const glm::dvec3& oldRight =
        oldCameraBasis.right;

    const glm::dvec3& newDirection =
        newCameraBasis.direction;

    const glm::dvec3& newUp =
        newCameraBasis.up;

    const glm::dvec3& newRight =
        newCameraBasis.right;

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

void SystemMapView::zoomCameraAroundPivot(
    const glm::dvec3& pivotAbsolute,
    float zoomFactor,
    float minimumDistance,
    float maximumDistance,
    double minimumEyeDistanceFromPivot
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

void SystemMapView::beginCameraFlight(
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

void SystemMapView::updateCameraFlight(
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

void SystemMapView::cancelCameraFlight(
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

float SystemMapView::navigationAnchorDiameterPx(
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

void SystemMapView::updateNavigationHoverPresentation(
    const Viewport& viewport,
    double nowSeconds
)
{
    // Visual hover lifetime of the logical (resolver) hover: fade in / out.
    (void)viewport;
    updateHierarchyHoverFade(
        {&m_state.hoverVisualCell, &m_state.hoverVisualAlpha, &m_state.hoverOutgoingCell,
         &m_state.hoverOutgoingAlpha, &m_state.hoverVisualLastTimeSeconds},
        m_state.navigationGrid.hasHoveredCell()
            ? std::optional<navigation::CubicNavigationCell>(m_state.navigationGrid.hoveredCell())
            : std::nullopt,
        nowSeconds,
        m_controls.navigationHoverFadeInSeconds,
        m_controls.navigationHoverFadeOutSeconds
    );
}

std::optional<navigation::CubicNavigationCell>
SystemMapView::resolvedTerminalSelection() const
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

float SystemMapView::navigationMaximumCameraDistance(
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

void SystemMapView::constrainCameraToNavigationBoundary(
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

float SystemMapView::wrapAngleRad(
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

float SystemMapView::depthHalfRange(
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

float SystemMapView::nearPlane(
    float cameraHalfHeight
)
{
    return std::max(
        depthHalfRange(cameraHalfHeight) *
            0.0001f,
        0.000000001f
    );
}

float SystemMapView::perspectiveEyeDistance(
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

float SystemMapView::farPlane(
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


} // namespace game::system_map

namespace game::system_map
{

// ============================================================================
// Spatial frame: the current parent cube (the same model as the Galaxy Map)
// ============================================================================

GalaxyHierarchyBox SystemMapView::hierarchyBox() const
{
    return cubicHierarchyBox(
        m_state.navigationGrid,
        m_state.hierarchyParent
    );
}

std::optional<navigation::CubicNavigationCell> SystemMapView::hierarchyChildAt(
    const glm::dvec3& positionAu
) const
{
    return galaxyHierarchyChildAt(
        m_state.navigationGrid,
        hierarchyBox(),
        positionAu
    );
}

bool SystemMapView::hierarchyContains(
    const glm::dvec3& positionAu
) const
{
    return galaxyHierarchyContains(hierarchyBox(), positionAu);
}

float SystemMapView::cubeScreenRatioAt(
    const glm::dvec3& centerAu,
    double edgeAu,
    const glm::dvec3& targetWorld,
    float halfHeight,
    const Viewport& viewport,
    bool fitInside
) const
{
    const OrbitCameraBasis basis =
        orbitCameraBasis(m_state.camera.yaw, m_state.camera.pitch);
    const float h = std::clamp(halfHeight, minimumCameraHalfHeight, maximumCameraHalfHeight);
    const double eyeDistance =
        static_cast<double>(perspectiveEyeDistance(h, m_visuals.projectionFieldOfViewDeg));
    const glm::mat4 view =
        glm::lookAt(glm::vec3(basis.direction * eyeDistance), glm::vec3(0.0f), glm::vec3(basis.up));
    const float aspect =
        viewport.height > 0 ? static_cast<float>(viewport.width) / static_cast<float>(viewport.height) : 1.0f;
    const glm::mat4 projection =
        glm::perspective(glm::radians(m_visuals.projectionFieldOfViewDeg), aspect, nearPlane(h),
                         farPlane(h, m_visuals.projectionFieldOfViewDeg));
    const glm::mat4 mvp = projection * view;
    const double scale = static_cast<double>(m_state.lastScale);

    float minX = 1.0e30f, minY = 1.0e30f, maxX = -1.0e30f, maxY = -1.0e30f;
    float largest = 0.0f;
    const double half = edgeAu * 0.5;
    for (int i = 0; i < 8; ++i)
    {
        const glm::dvec3 corner =
            centerAu + glm::dvec3((i & 1) ? half : -half, (i & 2) ? half : -half, (i & 4) ? half : -half);
        const glm::vec4 clip = mvp * glm::vec4(glm::vec3(corner * scale - targetWorld), 1.0f);
        if (clip.w <= 1.0e-9f)
            return 1.0e9f;
        minX = std::min(minX, clip.x / clip.w);
        maxX = std::max(maxX, clip.x / clip.w);
        minY = std::min(minY, clip.y / clip.w);
        maxY = std::max(maxY, clip.y / clip.w);
        largest = std::max({largest, std::abs(clip.x / clip.w), std::abs(clip.y / clip.w)});
    }
    if (fitInside)
        return largest;
    return std::max((maxX - minX) * 0.5f, (maxY - minY) * 0.5f);
}

float SystemMapView::cubeScreenRatio(
    const glm::dvec3& centerAu,
    double edgeAu,
    const Viewport& viewport
) const
{
    return cubeScreenRatioAt(centerAu, edgeAu, m_state.camera.target, m_state.camera.distance, viewport);
}

float SystemMapView::fillHalfHeightAt(
    const glm::dvec3& centerAu,
    double edgeAu,
    const glm::dvec3& targetWorld,
    const Viewport& viewport,
    float fill
) const
{
    // The screen ratio falls monotonically with the half-height: bisection
    // in log space over the camera range.
    double inner = std::log(static_cast<double>(minimumCameraHalfHeight));
    double outer = std::log(static_cast<double>(maximumCameraHalfHeight));
    for (int k = 0; k < 60; ++k)
    {
        const double mid = 0.5 * (inner + outer);
        if (cubeScreenRatioAt(centerAu, edgeAu, targetWorld, static_cast<float>(std::exp(mid)), viewport, true) >
            fill)
        {
            inner = mid;
        }
        else
        {
            outer = mid;
        }
    }
    return static_cast<float>(std::exp(outer));
}

float SystemMapView::hierarchyFitHalfHeight(
    const Viewport& viewport
) const
{
    const GalaxyHierarchyBox box = hierarchyBox();
    return fillHalfHeightAt(
        box.centerLy,
        box.edgeLy,
        box.centerLy * static_cast<double>(m_state.lastScale),
        viewport,
        ParentFillRatio
    );
}

void SystemMapView::hierarchyHalfHeightLimits(
    const Viewport& viewport,
    float& minimumHalfHeight,
    float& maximumHalfHeight
) const
{
    /*
        Deep enough for a child to fill the screen (it then becomes the
        parent); the S0 root never gets smaller than the screen, any other
        parent hands over to its own parent before.
    */
    const GalaxyHierarchyBox box = hierarchyBox();
    const float fitted = hierarchyFitHalfHeight(viewport);
    const float kmLimited =
        static_cast<float>(
            (m_controls.minKmPerPixel * static_cast<double>(m_state.lastScale) / 149597870.7) *
            static_cast<double>(viewport.height) * 0.5);
    minimumHalfHeight = std::max({minimumCameraHalfHeight, kmLimited, fitted * 0.02f});
    minimumHalfHeight = std::min(minimumHalfHeight, fitted);
    maximumHalfHeight =
        box.parent.level <= 0
            ? fillHalfHeightAt(box.centerLy, box.edgeLy, m_state.camera.target, viewport, ParentFillRatio)
            : std::min(maximumCameraHalfHeight, fitted * 1.6f);
    maximumHalfHeight = std::max(maximumHalfHeight, minimumHalfHeight);
}

void SystemMapView::setHierarchyParent(
    const GalaxyHierarchyParent& parent
)
{
    m_state.hierarchyParent = parent;
    ++m_state.hierarchyRevision;

    auto& grid = m_state.navigationGrid;
    const GalaxyHierarchyBox box = hierarchyBox();
    grid.setAnchor(box.childLevel, grid.nearestIndexForPosition(box.centerLy, box.childLevel));
    grid.clearHoveredCell();

    m_state.hoverVisualCell.reset();
    m_state.hoverVisualAlpha = 0.0f;
    m_state.hoverOutgoingCell.reset();
    m_state.hoverOutgoingAlpha = 0.0f;
    m_state.hoverTarget = SystemPointerTarget {};
    m_state.hoverTargetValid = false;
    m_state.cubeClickTracker.reset();
    m_state.bodyClickTracker.reset();
    m_state.hierarchyFaces.valid = false;
}

void SystemMapView::frameHierarchyParent(
    const Viewport& viewport,
    double nowSeconds,
    bool animate
)
{
    const GalaxyHierarchyBox box = hierarchyBox();
    const glm::dvec3 target = box.centerLy * static_cast<double>(m_state.lastScale);
    const float halfHeight = hierarchyFitHalfHeight(viewport);
    if (animate)
    {
        beginCameraFlight(target, halfHeight, nowSeconds);
        return;
    }
    cancelCameraFlight(false);
    m_state.camera.target = target;
    m_state.camera.distance = halfHeight;
}

bool SystemMapView::enterHierarchyCell(
    const navigation::CubicNavigationCell& cell,
    const Viewport& viewport,
    double nowSeconds,
    bool frameCamera
)
{
    auto& grid = m_state.navigationGrid;
    if (cell.level < 0 || cell.level >= grid.maximumLevel() || !grid.isCellNavigable(cell))
        return false;

    GalaxyHierarchyParent parent;
    parent.level = cell.level;
    parent.index = cell.index;
    setHierarchyParent(parent);
    // The entered cube stays the selected address until a child is chosen.
    grid.selectCell(grid.cell(cell.index, cell.level));

    if (frameCamera)
        frameHierarchyParent(viewport, nowSeconds, true);
    return true;
}

bool SystemMapView::ascendHierarchy(
    const Viewport& viewport,
    double nowSeconds
)
{
    return jumpToHierarchyLevel(m_state.hierarchyParent.level - 1, viewport, nowSeconds);
}

bool SystemMapView::jumpToHierarchyLevel(
    int level,
    const Viewport& viewport,
    double nowSeconds,
    bool frameCamera
)
{
    const GalaxyHierarchyParent current = m_state.hierarchyParent;
    if (level < 0 || level >= current.level)
        return false;

    auto& grid = m_state.navigationGrid;
    const navigation::CubicNavigationCell currentCell = grid.cell(current.index, current.level);
    // The cube the user leaves stays visible and selected inside the parent.
    const navigation::CubicNavigationCell cameFrom =
        grid.cell(grid.ancestorIndex(currentCell.index, currentCell.level, level + 1), level + 1);

    setHierarchyParent(galaxyHierarchyAncestor(grid, currentCell, level));
    if (m_state.selectedBodyId.empty() && m_state.selectedHubId.empty())
    {
        grid.selectCell(cameFrom);
        m_state.navigationCellExplicitlySelected = true;
    }

    if (frameCamera)
        frameHierarchyParent(viewport, nowSeconds, true);
    return true;
}

void SystemMapView::resetHierarchyToRoot(
    const Viewport& viewport
)
{
    setHierarchyParent(GalaxyHierarchyParent {0, {}});
    frameHierarchyParent(viewport, 0.0, false);
}

} // namespace game::system_map
