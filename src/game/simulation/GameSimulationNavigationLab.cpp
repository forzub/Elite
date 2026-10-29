// GameSimulation: DEV navigation runtime lab (lab initialisation, static geometry publication, lab intent planning).

#include "src/game/simulation/GameSimulationInternal.h"




void GameSimulation::initializeNavigationRuntimeLab()
{
    using namespace game::diagnostics;
    using Map = world::navigation::NavigationMap;
    using Space = world::navigation::NavigationSpace;

    if (!NavigationRuntimeLabEnabled ||
        m_navigationRuntimeLabInitialized ||
        m_navigationRuntimeLabShipId.value == 0 ||
        m_navigationRuntimeLabHubId.empty())
    {
        return;
    }

    const auto* hubFrame =
        hubNavigationFrame(m_navigationRuntimeLabHubId);
    if (!hubFrame || !hubFrame->valid)
        return;

    game::navigation::ReferenceFrame startFrame;
    startFrame.type =
        game::navigation::ReferenceFrameType::OrbitalHub;
    startFrame.systemId = hubFrame->systemId;
    startFrame.hubId = m_navigationRuntimeLabHubId;
    startFrame.localOffsetMeters =
        NavigationRuntimeLabStartTacticalLocalMeters;

    if (!placeShipInReferenceFrame(
            m_navigationRuntimeLabShipId,
            startFrame))
    {
        return;
    }

    const Ship* placedLabShip =
        getShip(m_navigationRuntimeLabShipId);
    if (!placedLabShip)
        return;

    const glm::dvec3 placedWorldMeters =
        world::coordinates::fullMeters(
            placedLabShip->core().transform().worldPosition
        );
    const auto navigationBoundary =
        makeNavigationRuntimeLabBoundary(*hubFrame);
    if (!navigationBoundary.valid())
        return;

    const glm::dvec3 placementPositionMap =
        navigationBoundary.toNavigation(
            game::navigation::NavigationFrameBoundary::SystemPosition {
                placedWorldMeters
            }
        ).meters;
    const glm::dvec3 placementMotionLocal =
        placedLabShip->core().transform().motion.localPositionMeters;
    const double placementServerTimeSeconds =
        m_serverTimelineClock.timeSeconds();

    Map::Config mapConfig;
    mapConfig.halfExtentMeters =
        NavigationRuntimeLabWorkspaceHalfExtentMeters;
    mapConfig.cellSizeMeters = 400.0;
    mapConfig.predictionHorizonSeconds = 3.0;
    mapConfig.interactionMarginMeters = 30.0;

    m_navigationRuntimeLabMap =
        std::make_unique<Map>(mapConfig);
    m_navigationRuntimeLabSpace =
        std::make_unique<Space>();

    Space::StaticSpaceUpdate staticWorld;
    staticWorld.sourceRevision = 1;

    Space::RegionInput approachRegion;
    approachRegion.regionId = 1;
    approachRegion.boundsMapMeters.minMapMeters = {
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        -NavigationRuntimeLabWorkspaceHalfExtentMeters
    };
    approachRegion.boundsMapMeters.maxMapMeters = {
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.z
    };
    approachRegion.clearanceRadiusMeters =
        NavigationRuntimeLabWorkspaceHalfExtentMeters;
    approachRegion.geometryRevision = 1;

    Space::RegionInput tunnelRegion;
    tunnelRegion.regionId = 2;
    tunnelRegion.boundsMapMeters.minMapMeters = {
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x -
            NavigationRuntimeLabSlitHalfWidthMeters,
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y -
            NavigationRuntimeLabSlitHalfHeightMeters,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.z
    };
    tunnelRegion.boundsMapMeters.maxMapMeters = {
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x +
            NavigationRuntimeLabSlitHalfWidthMeters,
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y +
            NavigationRuntimeLabSlitHalfHeightMeters,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.z
    };
    tunnelRegion.clearanceRadiusMeters =
        NavigationRuntimeLabSlitPortalClearanceMeters;
    tunnelRegion.geometryRevision = 1;

    Space::RegionInput departureRegion;
    departureRegion.regionId = 3;
    departureRegion.boundsMapMeters.minMapMeters = {
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.z
    };
    departureRegion.boundsMapMeters.maxMapMeters = {
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabWorkspaceHalfExtentMeters
    };
    departureRegion.clearanceRadiusMeters =
        NavigationRuntimeLabWorkspaceHalfExtentMeters;
    departureRegion.geometryRevision = 1;

    staticWorld.regions.push_back(approachRegion);
    staticWorld.regions.push_back(tunnelRegion);
    staticWorld.regions.push_back(departureRegion);

    Space::PortalInput slitEntryPortal;
    slitEntryPortal.portalId = NavigationRuntimeLabSlitEntryPortalId;
    slitEntryPortal.regionA = 1;
    slitEntryPortal.regionB = 2;
    slitEntryPortal.centerMapMeters = {
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.x,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.y,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.z
    };
    slitEntryPortal.clearanceRadiusMeters =
        NavigationRuntimeLabSlitPortalClearanceMeters;
    slitEntryPortal.bidirectional = true;
    slitEntryPortal.geometryRevision = 1;
    slitEntryPortal.traversal.enabled = true;
    slitEntryPortal.traversal.normalAToBMap = {0.0, 0.0, 1.0};
    slitEntryPortal.traversal.approachDistanceMeters =
        NavigationRuntimeLabSlitApproachDistanceMeters;
    slitEntryPortal.traversal.maximumVelocityAngleRad =
        NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad;
    slitEntryPortal.traversal.maximumForwardAngleRad =
        NavigationRuntimeLabSlitMaximumEntryForwardAngleRad;
    slitEntryPortal.traversal.maximumLateralSpeedMps =
        NavigationRuntimeLabSlitMaximumLateralSpeedMps;
    slitEntryPortal.traversal.transitSpeedMps =
        NavigationRuntimeLabSlitTransitSpeedMps;
    slitEntryPortal.traversal.requireVehicleForwardAlignment = true;
    staticWorld.portals.push_back(slitEntryPortal);

    Space::PortalInput slitExitPortal;
    slitExitPortal.portalId = NavigationRuntimeLabSlitExitPortalId;
    slitExitPortal.regionA = 2;
    slitExitPortal.regionB = 3;
    slitExitPortal.centerMapMeters = {
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.x,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.y,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.z
    };
    slitExitPortal.clearanceRadiusMeters =
        NavigationRuntimeLabSlitPortalClearanceMeters;
    slitExitPortal.bidirectional = true;
    slitExitPortal.geometryRevision = 1;
    slitExitPortal.traversal.enabled = true;
    slitExitPortal.traversal.normalAToBMap = {0.0, 0.0, 1.0};
    slitExitPortal.traversal.approachDistanceMeters = 0.0;
    slitExitPortal.traversal.maximumVelocityAngleRad =
        NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad;
    slitExitPortal.traversal.maximumForwardAngleRad =
        NavigationRuntimeLabSlitMaximumEntryForwardAngleRad;
    slitExitPortal.traversal.maximumLateralSpeedMps =
        NavigationRuntimeLabSlitMaximumLateralSpeedMps;
    slitExitPortal.traversal.transitSpeedMps =
        NavigationRuntimeLabSlitTransitSpeedMps;
    slitExitPortal.traversal.requireVehicleForwardAlignment = true;
    staticWorld.portals.push_back(slitExitPortal);

    m_navigationRuntimeLabSpace->replaceStaticWorld(
        std::move(staticWorld)
    );

    m_navigationRuntimeLabSourceRevision = 0;
    m_navigationRuntimeLabLastPlan = {};
    m_navigationRuntimeLabAcceptedSegment = {};
    m_navigationRuntimeLabWorkScheduler =
        game::navigation::NavigationWorkScheduler {};
    m_navigationRuntimeLabNextPlannerJobRevision = 1;
    m_navigationRuntimeLabCapabilityRevision = 1;
    m_navigationRuntimeLabSchedulerCapabilityInitialized = false;
    m_navigationRuntimeLabSchedulerCapability = {};
    m_navigationRuntimeLabNextSegmentRevision = 1;
    m_navigationRuntimeLabObservation = {};
    m_navigationRuntimeLabObservation.valid = true;
    m_navigationRuntimeLabObservation.shipEntityId =
        m_navigationRuntimeLabShipId.value;
    m_navigationRuntimeLabObservation.placementProbeCaptured = true;
    m_navigationRuntimeLabObservation.placementPositionMap =
        placementPositionMap;
    m_navigationRuntimeLabObservation.placementMotionLocalTactical =
        placementMotionLocal;
    m_navigationRuntimeLabObservation.placementServerTimeSeconds =
        placementServerTimeSeconds;
    m_navigationRuntimeLabStaticGeometryPublished = false;
    m_navigationRuntimeLabInitialized = true;
}


void GameSimulation::publishNavigationRuntimeLabStaticGeometry()
{
    using namespace game::diagnostics;
    using Space = world::navigation::NavigationSpace;

    if (!NavigationRuntimeLabEnabled ||
        !m_navigationRuntimeLabInitialized ||
        m_navigationRuntimeLabStaticGeometryPublished ||
        !m_navigationRuntimeLabSpace)
    {
        return;
    }

    const auto* hubFrame =
        hubNavigationFrame(m_navigationRuntimeLabHubId);
    if (!hubFrame || !hubFrame->valid)
        return;

    const auto navigationBoundary =
        makeNavigationRuntimeLabBoundary(*hubFrame);
    if (!navigationBoundary.valid())
        return;

    Space::StaticSpaceUpdate staticWorld;
    staticWorld.sourceRevision = 2;

    Space::RegionInput approachRegion;
    approachRegion.regionId = 1;
    approachRegion.boundsMapMeters.minMapMeters = {
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        -NavigationRuntimeLabWorkspaceHalfExtentMeters
    };
    approachRegion.boundsMapMeters.maxMapMeters = {
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.z
    };
    approachRegion.clearanceRadiusMeters =
        NavigationRuntimeLabWorkspaceHalfExtentMeters;
    approachRegion.geometryRevision = 2;

    Space::RegionInput tunnelRegion;
    tunnelRegion.regionId = 2;
    tunnelRegion.boundsMapMeters.minMapMeters = {
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x -
            NavigationRuntimeLabSlitHalfWidthMeters,
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y -
            NavigationRuntimeLabSlitHalfHeightMeters,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.z
    };
    tunnelRegion.boundsMapMeters.maxMapMeters = {
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x +
            NavigationRuntimeLabSlitHalfWidthMeters,
        NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y +
            NavigationRuntimeLabSlitHalfHeightMeters,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.z
    };
    tunnelRegion.clearanceRadiusMeters =
        NavigationRuntimeLabSlitPortalClearanceMeters;
    tunnelRegion.geometryRevision = 2;

    Space::RegionInput departureRegion;
    departureRegion.regionId = 3;
    departureRegion.boundsMapMeters.minMapMeters = {
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        -NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.z
    };
    departureRegion.boundsMapMeters.maxMapMeters = {
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabWorkspaceHalfExtentMeters,
        NavigationRuntimeLabWorkspaceHalfExtentMeters
    };
    departureRegion.clearanceRadiusMeters =
        NavigationRuntimeLabWorkspaceHalfExtentMeters;
    departureRegion.geometryRevision = 2;

    staticWorld.regions.push_back(approachRegion);
    staticWorld.regions.push_back(tunnelRegion);
    staticWorld.regions.push_back(departureRegion);

    Space::PortalInput slitEntryPortal;
    slitEntryPortal.portalId = NavigationRuntimeLabSlitEntryPortalId;
    slitEntryPortal.regionA = 1;
    slitEntryPortal.regionB = 2;
    slitEntryPortal.centerMapMeters = {
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.x,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.y,
        NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.z
    };
    slitEntryPortal.clearanceRadiusMeters =
        NavigationRuntimeLabSlitPortalClearanceMeters;
    slitEntryPortal.bidirectional = true;
    slitEntryPortal.geometryRevision = 2;
    slitEntryPortal.traversal.enabled = true;
    slitEntryPortal.traversal.normalAToBMap = {0.0, 0.0, 1.0};
    slitEntryPortal.traversal.approachDistanceMeters =
        NavigationRuntimeLabSlitApproachDistanceMeters;
    slitEntryPortal.traversal.maximumVelocityAngleRad =
        NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad;
    slitEntryPortal.traversal.maximumForwardAngleRad =
        NavigationRuntimeLabSlitMaximumEntryForwardAngleRad;
    slitEntryPortal.traversal.maximumLateralSpeedMps =
        NavigationRuntimeLabSlitMaximumLateralSpeedMps;
    slitEntryPortal.traversal.transitSpeedMps =
        NavigationRuntimeLabSlitTransitSpeedMps;
    slitEntryPortal.traversal.requireVehicleForwardAlignment = true;
    staticWorld.portals.push_back(slitEntryPortal);

    Space::PortalInput slitExitPortal;
    slitExitPortal.portalId = NavigationRuntimeLabSlitExitPortalId;
    slitExitPortal.regionA = 2;
    slitExitPortal.regionB = 3;
    slitExitPortal.centerMapMeters = {
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.x,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.y,
        NavigationRuntimeLabSlitExitCenterVisualLocalMeters.z
    };
    slitExitPortal.clearanceRadiusMeters =
        NavigationRuntimeLabSlitPortalClearanceMeters;
    slitExitPortal.bidirectional = true;
    slitExitPortal.geometryRevision = 2;
    slitExitPortal.traversal.enabled = true;
    slitExitPortal.traversal.normalAToBMap = {0.0, 0.0, 1.0};
    slitExitPortal.traversal.approachDistanceMeters = 0.0;
    slitExitPortal.traversal.maximumVelocityAngleRad =
        NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad;
    slitExitPortal.traversal.maximumForwardAngleRad =
        NavigationRuntimeLabSlitMaximumEntryForwardAngleRad;
    slitExitPortal.traversal.maximumLateralSpeedMps =
        NavigationRuntimeLabSlitMaximumLateralSpeedMps;
    slitExitPortal.traversal.transitSpeedMps =
        NavigationRuntimeLabSlitTransitSpeedMps;
    slitExitPortal.traversal.requireVehicleForwardAlignment = true;
    staticWorld.portals.push_back(slitExitPortal);

    std::uint32_t provingObstacleEntityId = 0;

    for (const auto& [objectId, object] : m_staticObjects)
    {
        if (object.systemId != m_activeCelestialSystemId ||
            !object.attachedToHub ||
            object.hubId != m_navigationRuntimeLabHubId ||
            object.ownerName != "Hub Motion Lab")
        {
            continue;
        }

        if (object.displayName == NavigationRuntimeLabObstacleLabel)
        {
            provingObstacleEntityId = objectId.value;
            m_navigationRuntimeLabObservation.obstacleEntityId =
                objectId.value;
        }

        // The static precision layer is intentionally static in the published
        // map frame. Self-rotating infrastructure and the deterministic
        // translating 12A-6b3b gap boundaries remain exclusively in the
        // dynamic/moving-passage domain.
        if (glm::length(object.hubLocalAngularVelocityDegPerSecond) > 1.0e-9 ||
            game::diagnostics::isNavigationRuntimeLabMovingGapBoundary(
                object.displayName
            ))
        {
            continue;
        }

        glm::dmat3 objectBasis(1.0);
        for (int axis = 0; axis < 3; ++axis)
        {
            const glm::vec4 column = object.orientation[axis];
            objectBasis[axis] = glm::dvec3(
                static_cast<double>(column.x),
                static_cast<double>(column.y),
                static_cast<double>(column.z)
            );
        }

        const glm::dvec3 objectWorldPosition =
            world::coordinates::fullMeters(object.worldPosition);

        auto exactObstacles =
            game::navigation::NavigationHitVolumeAdapter::buildObstacles(
                object.hitComponent,
                objectId.value,
                objectWorldPosition,
                objectBasis,
                object.displayName
            );

        for (auto& obstacle : exactObstacles)
        {
            obstacle.centerMeters =
                navigationBoundary.toNavigation(
                    game::navigation::NavigationFrameBoundary::SystemPosition {
                        obstacle.centerMeters
                    }
                ).meters;

            glm::dmat3 mapBasis(1.0);
            for (int axis = 0; axis < 3; ++axis)
            {
                mapBasis[axis] =
                    navigationBoundary.toNavigationVector(
                        game::navigation::NavigationFrameBoundary::SystemVector {
                            obstacle.localToWorldBasis[axis]
                        }
                    ).value;
            }
            obstacle.localToWorldBasis = mapBasis;

            staticWorld.obstacles.push_back(std::move(obstacle));
        }
    }

    // Do not mark publication complete before real physical geometry exists.
    // This permits first-tick HitVolume rebuild to finish before the immutable
    // map-frame static snapshot is frozen.
    if (staticWorld.obstacles.empty())
        return;

    m_navigationRuntimeLabSpace->replaceStaticWorld(
        std::move(staticWorld)
    );

    if (provingObstacleEntityId != 0)
    {
        Space::SegmentQuery configuredRoute;
        configuredRoute.startMapMeters = {
            NavigationRuntimeLabStartVisualLocalMeters.x,
            NavigationRuntimeLabStartVisualLocalMeters.y,
            NavigationRuntimeLabStartVisualLocalMeters.z
        };
        configuredRoute.endMapMeters = {
            NavigationRuntimeLabGoalVisualLocalMeters.x,
            NavigationRuntimeLabGoalVisualLocalMeters.y,
            NavigationRuntimeLabGoalVisualLocalMeters.z
        };
        configuredRoute.envelope.radiusMeters = 0.0;
        configuredRoute.envelope.additionalClearanceMeters = 0.0;
        configuredRoute.requireSameRegion = false;

        const Space::SegmentQueryResult fixtureProof =
            m_navigationRuntimeLabSpace->querySegment(configuredRoute);

        m_navigationRuntimeLabObservation.
            configuredRouteExactObstacleBlockPublished =
            !fixtureProof.traversable &&
            fixtureProof.blockingObstacleEntityId ==
                provingObstacleEntityId;
    }

    // Independently prove that the authored slit itself is real exact-static
    // free space for the current lab ship envelope. This is a fixture validity
    // check, not planner authority: the route planner still has to select the
    // portal and the physical ship still has to cross it.
    if (const Ship* labShip = getShip(m_navigationRuntimeLabShipId))
    {
        double labShipRadius =
            game::navigation::NavigationHitVolumeAdapter::
                conservativeRadiusFromOrigin(
                    labShip->core().hitComponent()
                );

        if (!(labShipRadius > 0.0) || !std::isfinite(labShipRadius))
        {
            const auto& dimensions =
                labShip->core().descriptor().logicalDimensions();
            const glm::dvec3 half(
                0.5 * static_cast<double>(dimensions.width),
                0.5 * static_cast<double>(dimensions.height),
                0.5 * static_cast<double>(dimensions.length)
            );
            labShipRadius = glm::length(half);
        }

        const glm::dvec3 slitNormal(0.0, 0.0, 1.0);
        const glm::dvec3 approachPoint =
            NavigationRuntimeLabSlitEntryCenterVisualLocalMeters -
            slitNormal *
                NavigationRuntimeLabSlitApproachDistanceMeters;
        const glm::dvec3 beyondExit =
            NavigationRuntimeLabSlitExitCenterVisualLocalMeters +
            slitNormal * 50.0;

        Space::SegmentQuery slitGeometryProof;
        slitGeometryProof.startMapMeters = {
            approachPoint.x,
            approachPoint.y,
            approachPoint.z
        };
        slitGeometryProof.endMapMeters = {
            beyondExit.x,
            beyondExit.y,
            beyondExit.z
        };
        slitGeometryProof.envelope.radiusMeters = labShipRadius;
        slitGeometryProof.envelope.additionalClearanceMeters = 10.0;
        slitGeometryProof.exactObstaclesOnly = true;

        const Space::SegmentQueryResult geometryProof =
            m_navigationRuntimeLabSpace->querySegment(
                slitGeometryProof
            );

        m_navigationRuntimeLabObservation.slitPortalExactObstaclesExamined =
            geometryProof.obstaclesExamined;
        m_navigationRuntimeLabObservation.slitPortalExactOpenPublished =
            geometryProof.traversable;
    }

    const auto stats = m_navigationRuntimeLabSpace->stats();
    m_navigationRuntimeLabObservation.exactStaticGeometryPublished = true;
    m_navigationRuntimeLabObservation.exactStaticObstacleCount =
        stats.obstacleCount;
    m_navigationRuntimeLabStaticGeometryPublished = true;
}


bool GameSimulation::buildNavigationRuntimeLabIntent(
    EntityId id,
    Ship& ship,
    game::navigation::NavigationRuntimeControlBridge::Intent& outIntent
)
{
    using namespace game::diagnostics;
    using Planner = game::navigation::NavigationRuntimePlanner;
    using Map = world::navigation::NavigationMap;
    using Space = world::navigation::NavigationSpace;

    if (!NavigationRuntimeLabEnabled ||
        !m_navigationRuntimeLabInitialized ||
        id != m_navigationRuntimeLabShipId ||
        !m_navigationRuntimeLabMap ||
        !m_navigationRuntimeLabSpace)
    {
        return false;
    }

    const auto* hubFrame =
        hubNavigationFrame(m_navigationRuntimeLabHubId);
    if (!hubFrame || !hubFrame->valid)
        return false;

    const auto navigationBoundary =
        makeNavigationRuntimeLabBoundary(*hubFrame);
    if (!navigationBoundary.valid())
        return false;

    Map::DynamicWorldUpdate dynamicWorld;
    dynamicWorld.sourceRevision =
        ++m_navigationRuntimeLabSourceRevision;

    glm::dvec3 observedObstacleWorldPosition(0.0);
    double observedObstacleRadiusMeters = 0.0;
    bool haveObservedObstacle = false;

    std::uint32_t rotatingActorEntityId = 0;
    glm::dvec3 rotatingActorMapPosition(0.0);
    glm::dvec3 expectedRotatingActorAngularVelocityMapRadPerSecond(0.0);
    bool haveRotatingActor = false;

    std::uint32_t movingGapUpperEntityId = 0;
    std::uint32_t movingGapLowerEntityId = 0;
    glm::dvec3 movingGapUpperMapPosition(0.0);
    glm::dvec3 movingGapLowerMapPosition(0.0);
    const glm::dvec3 expectedMovingGapVelocityMapMps =
        NavigationRuntimeLabMovingGapVelocityVisualMps;

    // Stage 12 lab deliberately consumes the already-spawned physical
    // NAV STRESS / GUIDANCE objects. Their damage HitComponent is the geometry
    // source; no render-mesh or presentation-only obstacle list is consulted.
    for (const auto& [objectId, object] : m_staticObjects)
    {
        if (object.systemId != hubFrame->systemId ||
            !object.attachedToHub ||
            object.hubId != m_navigationRuntimeLabHubId ||
            object.ownerName != "Hub Motion Lab")
        {
            continue;
        }

        double radius =
            game::navigation::NavigationHitVolumeAdapter::
                conservativeRadiusFromOrigin(object.hitComponent);

        if (!(radius > 0.0) || !std::isfinite(radius))
        {
            const auto& dimensions =
                ObjectDescriptorRegistry::get(object.type).
                    logicalDimensions();
            const glm::dvec3 half(
                0.5 * static_cast<double>(dimensions.width),
                0.5 * static_cast<double>(dimensions.height),
                0.5 * static_cast<double>(dimensions.length)
            );
            radius = glm::length(half);
        }

        if (!(radius > 0.0) || !std::isfinite(radius))
            continue;

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(object.worldPosition);

        if (object.displayName == NavigationRuntimeLabObstacleLabel)
        {
            m_navigationRuntimeLabObservation.obstacleEntityId =
                objectId.value;
            observedObstacleWorldPosition = positionMeters;
            observedObstacleRadiusMeters = radius;
            haveObservedObstacle = true;
        }

        // Stage 12A-5 ownership split:
        // - stationary infrastructure is exact NavigationSpace truth only;
        // - time-varying infrastructure remains in NavigationMap broadphase
        //   until the dedicated moving/rotating exact-geometry slice lands.
        //
        // CUBE 08 is stationary in the hub/map frame and MUST NOT be duplicated
        // here as a conservative sphere.
        const bool movingGapBoundary =
            isNavigationRuntimeLabMovingGapBoundary(
                object.displayName
            );
        const bool timeVaryingNavigationActor =
            movingGapBoundary ||
            glm::length(
                object.hubLocalAngularVelocityDegPerSecond
            ) > 1.0e-9;

        if (!timeVaryingNavigationActor)
            continue;

        Map::DynamicActorInput actor;
        actor.entityId = objectId.value;

        const game::navigation::NavigationFrameBoundary::SystemPosition
            objectSystemPosition {positionMeters};
        const game::navigation::NavigationFrameBoundary::SystemVelocity
            objectSystemVelocity {glm::dvec3(object.linearVelocity)};

        const glm::dvec3 objectPositionMap =
            navigationBoundary.toNavigation(
                objectSystemPosition
            ).meters;
        actor.positionMapMeters = {
            objectPositionMap.x,
            objectPositionMap.y,
            objectPositionMap.z
        };

        const glm::dvec3 objectVelocityMap =
            navigationBoundary.toNavigation(
                objectSystemPosition,
                objectSystemVelocity
            ).metersPerSecond;
        actor.velocityMapMetersPerSecond = {
            objectVelocityMap.x,
            objectVelocityMap.y,
            objectVelocityMap.z
        };

        // These lab boundaries have constant NavLocal linear velocity. Their
        // system-space acceleration is not separately published by StaticObject,
        // so publish the known local acceleration truth instead of fabricating
        // a "system acceleration" DTO.
        actor.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};

        if (object.displayName == NavigationRuntimeLabMovingGapUpperLabel)
        {
            movingGapUpperEntityId = objectId.value;
            movingGapUpperMapPosition = objectPositionMap;
            m_navigationRuntimeLabObservation.movingGapUpperEntityId =
                objectId.value;
        }
        else if (object.displayName ==
                 NavigationRuntimeLabMovingGapLowerLabel)
        {
            movingGapLowerEntityId = objectId.value;
            movingGapLowerMapPosition = objectPositionMap;
            m_navigationRuntimeLabObservation.movingGapLowerEntityId =
                objectId.value;
        }

        const glm::dvec3 angularVelocityMapRadPerSecond =
            navigationBoundary.toNavigation(
                game::navigation::NavigationFrameBoundary::SystemAngularVelocity {
                    glm::dvec3(object.angularVelocity)
                }
            ).radiansPerSecond;
        actor.angularVelocityMapRadPerSecond = {
            angularVelocityMapRadPerSecond.x,
            angularVelocityMapRadPerSecond.y,
            angularVelocityMapRadPerSecond.z
        };

        // NavigationMap's radius is candidate broadphase only. Preserve the
        // authoritative current HitVolume OBBs by value so translation-only
        // moving infrastructure can receive an exact dynamic narrow phase.
        const auto dynamicWorldObstacles =
            game::navigation::NavigationHitVolumeAdapter::buildObstacles(
                object.hitComponent,
                objectId.value,
                positionMeters,
                glm::dmat3(object.orientation),
                "dynamic_nav_" + std::to_string(objectId.value)
            );
        actor.exactObstacles.reserve(dynamicWorldObstacles.size());
        for (auto obstacle : dynamicWorldObstacles)
        {
            obstacle.centerMeters =
                navigationBoundary.toNavigation(
                    game::navigation::NavigationFrameBoundary::SystemPosition {
                        obstacle.centerMeters
                    }
                ).meters;

            for (int axis = 0; axis < 3; ++axis)
            {
                obstacle.localToWorldBasis[axis] =
                    navigationBoundary.toNavigationVector(
                        game::navigation::NavigationFrameBoundary::SystemVector {
                            obstacle.localToWorldBasis[axis]
                        }
                    ).value;
            }

            actor.exactObstacles.push_back(std::move(obstacle));
        }

        if (object.displayName == NavigationRuntimeLabRotatingActorLabel)
        {
            rotatingActorEntityId = objectId.value;
            rotatingActorMapPosition = objectPositionMap;
            expectedRotatingActorAngularVelocityMapRadPerSecond =
                glm::radians(object.hubLocalAngularVelocityDegPerSecond);
            haveRotatingActor = true;

            m_navigationRuntimeLabObservation.rotatingActorEntityId =
                objectId.value;
            m_navigationRuntimeLabObservation.
                expectedRotatingActorAngularVelocityMapRadPerSecond =
                expectedRotatingActorAngularVelocityMapRadPerSecond;
        }

        actor.radiusMeters = radius;
        actor.flags = 1u;
        actor.motionRevision = dynamicWorld.sourceRevision;
        dynamicWorld.actors.push_back(actor);
    }

    m_navigationRuntimeLabMap->replaceDynamicWorld(
        std::move(dynamicWorld)
    );

    if (movingGapUpperEntityId != 0 &&
        movingGapLowerEntityId != 0)
    {
        auto& observation = m_navigationRuntimeLabObservation;
        observation.movingGapCurrentCenterMap =
            0.5 * (movingGapUpperMapPosition + movingGapLowerMapPosition);

        double maximumVelocityError = 0.0;
        bool upperVerified = false;
        bool lowerVerified = false;

        const auto verifyBoundary =
            [&](std::uint32_t entityId,
                const glm::dvec3& centerMap,
                bool& verified)
            {
                Map::SphereQuery query;
                query.centerMapMeters = {
                    centerMap.x,
                    centerMap.y,
                    centerMap.z
                };
                query.radiusMeters = 1.0;

                const Map::QueryResult result =
                    m_navigationRuntimeLabMap->querySphere(query);

                for (const auto& candidate : result.candidates)
                {
                    if (candidate.entityId != entityId)
                        continue;

                    const glm::dvec3 observedVelocity(
                        candidate.velocityMapMetersPerSecond.x,
                        candidate.velocityMapMetersPerSecond.y,
                        candidate.velocityMapMetersPerSecond.z
                    );
                    const double error =
                        glm::length(
                            observedVelocity -
                            expectedMovingGapVelocityMapMps
                        );
                    maximumVelocityError =
                        std::max(maximumVelocityError, error);
                    verified = std::isfinite(error) &&
                        error <=
                            NavigationRuntimeLabLinearVelocityToleranceMps;
                    break;
                }
            };

        verifyBoundary(
            movingGapUpperEntityId,
            movingGapUpperMapPosition,
            upperVerified
        );
        verifyBoundary(
            movingGapLowerEntityId,
            movingGapLowerMapPosition,
            lowerVerified
        );

        observation.movingGapMaximumVelocityErrorMps =
            std::max(
                observation.movingGapMaximumVelocityErrorMps,
                maximumVelocityError
            );
        observation.movingGapKinematicsVerified =
            observation.movingGapKinematicsVerified ||
            (upperVerified && lowerVerified);
    }

    if (haveRotatingActor)
    {
        Map::SphereQuery rotatingActorQuery;
        rotatingActorQuery.centerMapMeters = {
            rotatingActorMapPosition.x,
            rotatingActorMapPosition.y,
            rotatingActorMapPosition.z
        };
        rotatingActorQuery.radiusMeters = 1.0;

        const Map::QueryResult rotatingActorResult =
            m_navigationRuntimeLabMap->querySphere(rotatingActorQuery);

        for (const auto& candidate : rotatingActorResult.candidates)
        {
            if (candidate.entityId != rotatingActorEntityId)
                continue;

            auto& observation = m_navigationRuntimeLabObservation;
            observation.rotatingActorCandidateSeen = true;
            observation.observedRotatingActorAngularVelocityMapRadPerSecond = {
                candidate.angularVelocityMapRadPerSecond.x,
                candidate.angularVelocityMapRadPerSecond.y,
                candidate.angularVelocityMapRadPerSecond.z
            };

            observation.rotatingActorAngularVelocityErrorRadPerSecond =
                glm::length(
                    observation.
                        observedRotatingActorAngularVelocityMapRadPerSecond -
                    observation.
                        expectedRotatingActorAngularVelocityMapRadPerSecond
                );

            observation.rotatingActorAngularVelocityVerified =
                std::isfinite(
                    observation.
                        rotatingActorAngularVelocityErrorRadPerSecond
                ) &&
                observation.rotatingActorAngularVelocityErrorRadPerSecond <=
                    NavigationRuntimeLabAngularVelocityToleranceRadPerSecond;
            break;
        }
    }

    const auto& transform = ship.core().transform();
    const glm::dvec3 shipWorldPosition =
        world::coordinates::fullMeters(transform.worldPosition);
    double shipRadius =
        game::navigation::NavigationHitVolumeAdapter::
            conservativeRadiusFromOrigin(
                ship.core().hitComponent()
            );

    if (!(shipRadius > 0.0) || !std::isfinite(shipRadius))
    {
        const auto& dimensions =
            ship.core().descriptor().logicalDimensions();
        const glm::dvec3 half(
            0.5 * static_cast<double>(dimensions.width),
            0.5 * static_cast<double>(dimensions.height),
            0.5 * static_cast<double>(dimensions.length)
        );
        shipRadius = glm::length(half);
    }

    const auto shipSystemPosition =
        game::navigation::NavigationFrameBoundary::SystemPosition {
            shipWorldPosition
        };
    const auto shipSystemVelocity =
        game::navigation::NavigationFrameBoundary::SystemVelocity {
            transform.motion.worldVelocityMps
        };
    const glm::dvec3 agentPositionMap =
        navigationBoundary.toNavigation(
            shipSystemPosition
        ).meters;
    const glm::dvec3 agentVelocityMap =
        navigationBoundary.toNavigation(
            shipSystemPosition,
            shipSystemVelocity
        ).metersPerSecond;

    const glm::dvec3 goalWorldPosition =
        hubFrame->localToWorldPosition(
            NavigationRuntimeLabGoalTacticalLocalMeters
        );
    const glm::dvec3 goalPositionMap =
        navigationBoundary.toNavigation(
            game::navigation::NavigationFrameBoundary::SystemPosition {
                goalWorldPosition
            }
        ).meters;

    const double labBrakingAccelerationMps2 =
        std::max(
            0.5,
            static_cast<double>(
                ship.core().desc().physics.manoeuvreThrusterAccel
            )
        );
    constexpr double LabTurnDistanceMeters = 1400.0;
    constexpr double LabSafetyMarginMeters = 20.0;
    constexpr double LabMinimumHorizonMeters = 100.0;

    const auto navigationPilotProfile =
        m_npcAiSystem.pilotSkillProfile(ship);
    const auto& navigationExecutionProfile =
        navigationPilotProfile.execution;

    const double navigationDecisionPeriodSeconds =
        navigationExecutionProfile.perceptionDecisionRateHz > 1.0e-9
            ? 1.0 /
                navigationExecutionProfile.perceptionDecisionRateHz
            : 0.0;
    const double navigationFilterResponseReserveSeconds =
        navigationExecutionProfile.responseFrequencyHz > 1.0e-9
            ? 1.0 /
                navigationExecutionProfile.responseFrequencyHz
            : 0.0;
    const double navigationControlResponseReserveSeconds =
        navigationExecutionProfile.reactionDelaySeconds +
        navigationDecisionPeriodSeconds +
        navigationExecutionProfile.commandLatencySeconds +
        navigationFilterResponseReserveSeconds;

    const double relativeSpeedMps =
        glm::length(agentVelocityMap);

    world::navigation::PhysicalManeuverHorizon::Query
        physicalHorizonQuery;
    physicalHorizonQuery.speedMetersPerSecond =
        relativeSpeedMps;
    physicalHorizonQuery.accelerationMagnitudeMetersPerSecond2 =
        0.0;
    physicalHorizonQuery.snapshotAgeSeconds = 0.0;
    physicalHorizonQuery.controlResponseReserveSeconds =
        navigationControlResponseReserveSeconds;
    physicalHorizonQuery.brakingAccelerationMetersPerSecond2 =
        labBrakingAccelerationMps2;
    physicalHorizonQuery.turnDistanceMeters =
        LabTurnDistanceMeters;
    physicalHorizonQuery.safetyMarginMeters =
        LabSafetyMarginMeters;
    physicalHorizonQuery.minimumDistanceMeters =
        LabMinimumHorizonMeters;
    physicalHorizonQuery.minimumLookAheadSeconds = 3.0;

    const auto physicalHorizon =
        world::navigation::PhysicalManeuverHorizon::evaluate(
            physicalHorizonQuery
        );
    if (!physicalHorizon.valid)
        return false;

    const double localHorizonMeters =
        physicalHorizon.distanceMeters;

    // The adjusted-target fan can leave the nominal route by hundreds of
    // metres. Query a bounded sphere covering the whole physical local
    // horizon, not only the original straight corridor; otherwise an adjusted
    // probe could miss an obstacle that was outside the nominal corridor.
    Map::SphereQuery dynamicQuery;
    dynamicQuery.centerMapMeters = {
        agentPositionMap.x,
        agentPositionMap.y,
        agentPositionMap.z
    };
    dynamicQuery.radiusMeters =
        localHorizonMeters +
        shipRadius +
        LabSafetyMarginMeters;
    dynamicQuery.lookAheadSeconds =
        physicalHorizon.lookAheadSeconds;

    const Map::QueryResult dynamicCandidates =
        m_navigationRuntimeLabMap->querySphere(dynamicQuery);

    ++m_navigationRuntimeLabObservation.dynamicQueryCount;
    m_navigationRuntimeLabObservation.maximumDynamicCandidateCount =
        std::max(
            m_navigationRuntimeLabObservation.maximumDynamicCandidateCount,
            dynamicCandidates.candidates.size()
        );

    if (m_navigationRuntimeLabObservation.obstacleEntityId != 0)
    {
        for (const auto& candidate : dynamicCandidates.candidates)
        {
            if (candidate.entityId ==
                m_navigationRuntimeLabObservation.obstacleEntityId)
            {
                m_navigationRuntimeLabObservation.obstacleCandidateSeen = true;
                break;
            }
        }
    }

    bool movingUpperCandidateSeen = false;
    bool movingLowerCandidateSeen = false;
    for (const auto& candidate : dynamicCandidates.candidates)
    {
        movingUpperCandidateSeen =
            movingUpperCandidateSeen ||
            (movingGapUpperEntityId != 0 &&
             candidate.entityId == movingGapUpperEntityId);
        movingLowerCandidateSeen =
            movingLowerCandidateSeen ||
            (movingGapLowerEntityId != 0 &&
             candidate.entityId == movingGapLowerEntityId);
    }
    m_navigationRuntimeLabObservation.movingGapPairCandidateSeen =
        m_navigationRuntimeLabObservation.movingGapPairCandidateSeen ||
        (movingUpperCandidateSeen && movingLowerCandidateSeen);

    Planner::AgentState agent;
    agent.entityId = id.value;
    agent.positionMapMeters = agentPositionMap;
    agent.velocityMapMetersPerSecond = agentVelocityMap;
    agent.accelerationMapMetersPerSecond2 = glm::dvec3(0.0);
    agent.radiusMeters = shipRadius;
    agent.forwardMap =
        navigationBoundary.toNavigationVector(
            game::navigation::NavigationFrameBoundary::SystemVector {
                glm::dvec3(transform.forward())
            }
        ).value;
    agent.rightMap =
        navigationBoundary.toNavigationVector(
            game::navigation::NavigationFrameBoundary::SystemVector {
                glm::dvec3(transform.right())
            }
        ).value;
    agent.upMap =
        navigationBoundary.toNavigationVector(
            game::navigation::NavigationFrameBoundary::SystemVector {
                glm::dvec3(transform.up())
            }
        ).value;
    agent.pitchRateRadPerSec =
        static_cast<double>(transform.pitchRate);
    agent.yawRateRadPerSec =
        static_cast<double>(transform.yawRate);
    agent.rollRateRadPerSec =
        static_cast<double>(transform.rollRate);

    const auto& shipDimensions =
        ship.core().descriptor().logicalDimensions();
    agent.hullHalfExtentsBodyMeters = {
        0.5 * static_cast<double>(shipDimensions.width),
        0.5 * static_cast<double>(shipDimensions.height),
        0.5 * static_cast<double>(shipDimensions.length)
    };

    const ShipParams shipPhysics = ship.core().effectivePhysics();
    const double forwardMainAuthorityMps2 =
        game::ship::forwardMainAccelerationLimitMps2(shipPhysics);
    const double reverseMainAuthorityMps2 =
        game::ship::reverseMainAccelerationLimitMps2(shipPhysics);
    const double manoeuvreAuthorityMps2 =
        game::ship::manoeuvreAccelerationLimitMps2(shipPhysics);

    agent.linearCapability.maxForwardAccelerationMetersPerSec2 =
        std::max(forwardMainAuthorityMps2, manoeuvreAuthorityMps2);
    agent.linearCapability.maxReverseAccelerationMetersPerSec2 =
        std::max(reverseMainAuthorityMps2, manoeuvreAuthorityMps2);
    agent.linearCapability.maxLateralAccelerationMetersPerSec2 =
        manoeuvreAuthorityMps2;
    agent.linearCapability.maxVerticalAccelerationMetersPerSec2 =
        manoeuvreAuthorityMps2;
    agent.angularCapability.maxAngularAccelerationRadPerSec2 =
        std::max(0.0, static_cast<double>(shipPhysics.angularAccel));
    agent.angularCapability.maxAngularSpeedRadPerSec =
        std::max(
            0.0,
            std::min({
                static_cast<double>(shipPhysics.maxPitchRate),
                static_cast<double>(shipPhysics.maxYawRate),
                static_cast<double>(shipPhysics.maxRollRate)
            })
        );
    agent.controlMode =
        transform.motion.localControlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted
            ? Planner::MovingPassage::ControlMode::EliteAssisted
            : Planner::MovingPassage::ControlMode::Newtonian;

    Planner::Goal plannerGoal;
    plannerGoal.revision = 1202001;
    plannerGoal.targetPositionMapMeters = goalPositionMap;
    plannerGoal.targetVelocityMapMetersPerSecond = glm::dvec3(0.0);
    plannerGoal.targetAccelerationMapMetersPerSecond2 = glm::dvec3(0.0);
    plannerGoal.maximumTargetSpeedMps =
        NavigationRuntimeLabMaximumSpeedMps;
    plannerGoal.velocityResponsePerSecond = 0.75;
    plannerGoal.angularDampingPerSecond = 2.0;
    plannerGoal.arrivalRadiusMeters =
        NavigationRuntimeLabArrivalRadiusMeters;

    Planner::Policy policy;
    policy.corridor.distanceWeight = 1.0;
    policy.corridor.preferredClearanceMultiple = 2.0;
    policy.corridor.clearancePenaltyMeters = 50.0;
    policy.corridor.turnPenaltyMetersPerRadian = 20.0;

    policy.horizon.lookAheadSeconds = 3.0;
    policy.horizon.controlResponseReserveSeconds =
        navigationControlResponseReserveSeconds;
    policy.horizon.maxResultAgeSeconds = 0.25;
    policy.horizon.maxBrakingAccelerationMetersPerSecond2 =
        labBrakingAccelerationMps2;
    policy.horizon.turnDistanceMeters =
        LabTurnDistanceMeters;
    policy.horizon.safetyMarginMeters =
        LabSafetyMarginMeters;
    policy.horizon.minimumHorizonMeters =
        LabMinimumHorizonMeters;

    policy.avoidance.lateralGridHalfExtentSamples = 8;
    policy.avoidance.minimumLateralStepMeters = 10.0;
    policy.avoidance.lateralStepEnvelopeMultiplier = 1.0;
    policy.avoidance.maximumLateralOffsetMeters = 800.0;
    policy.avoidance.longitudinalSamples = 3;
    policy.avoidance.projectionPaddingMeters = 10.0;
    policy.avoidance.trajectorySamples = 48;
    policy.avoidance.staticAdditionalClearanceMeters = 10.0;

    // Ordinary free-space encounter: do not force the ship through a gap just
    // because two dynamic actors happen to form a pair. The default authority
    // is projected visible-horizon bypass. Precision MovingPassage remains enabled
    // by explicit callers/fixtures that require a specific constrained gap.
    policy.movingPassage.enabled = false;
    policy.movingPassage.allowSteeringAuthority = false;
    policy.movingPassage.durationSeconds =
        NavigationRuntimeLabMovingPassageDurationSeconds;
    policy.movingPassage.hullAdditionalClearanceMeters = 2.0;
    policy.movingPassage.maximumAcceptedGapTravelAlignment = 0.5;
    policy.movingPassage.maximumInitialAngularRateRadPerSec = 0.05;
    policy.movingPassage.candidates.boundaryClearanceMeters = 0.0;
    policy.movingPassage.candidates.minimumClearSeparationMeters = 0.0;
    policy.movingPassage.candidates.maximumClearSeparationMeters = 2000.0;
    policy.movingPassage.candidates.secondaryClearanceMeters = 1000.0;
    policy.movingPassage.candidates.minimumForwardDistanceMeters = 0.0;
    policy.movingPassage.candidates.maximumForwardDistanceMeters = 1800.0;
    policy.movingPassage.candidates.maximumCenterlineOffsetMeters = 800.0;
    policy.movingPassage.candidates.maximumAbsSeparationTravelDot = 0.5;
    policy.movingPassage.prediction.secondaryClearanceMeters = 1000.0;
    policy.movingPassage.prediction.minimumContinuousClearSeparationMeters = 0.0;
    policy.movingPassage.prediction.maximumAbsSeparationTravelDot = 0.5;

    if (!m_navigationRuntimeLabObservation.firstLiveNominalProbeCaptured)
    {
        const glm::dvec3 nominalDelta =
            goalPositionMap - agentPositionMap;
        const double nominalDistance =
            glm::length(nominalDelta);

        glm::dvec3 boundedTarget =
            goalPositionMap;
        if (nominalDistance > localHorizonMeters &&
            nominalDistance > 1.0e-12)
        {
            boundedTarget =
                agentPositionMap +
                nominalDelta *
                    (localHorizonMeters / nominalDistance);
        }

        Space::SegmentQuery firstLiveProbe;
        firstLiveProbe.startMapMeters = {
            agentPositionMap.x,
            agentPositionMap.y,
            agentPositionMap.z
        };
        firstLiveProbe.endMapMeters = {
            boundedTarget.x,
            boundedTarget.y,
            boundedTarget.z
        };
        firstLiveProbe.envelope.radiusMeters = shipRadius;
        firstLiveProbe.envelope.additionalClearanceMeters =
            policy.avoidance.staticAdditionalClearanceMeters;
        firstLiveProbe.requireSameRegion = true;

        const Space::SegmentQueryResult firstLiveResult =
            m_navigationRuntimeLabSpace->querySegment(firstLiveProbe);

        auto& observation =
            m_navigationRuntimeLabObservation;
        observation.firstLiveNominalProbeCaptured = true;
        observation.firstLiveNominalExactBlocked =
            !firstLiveResult.traversable;
        observation.firstLiveNominalBlockingEntityId =
            firstLiveResult.blockingObstacleEntityId;
        observation.firstLiveHorizonMeters =
            localHorizonMeters;
        observation.firstLiveAgentPositionMap =
            agentPositionMap;
        observation.firstLiveGoalPositionMap =
            goalPositionMap;
        observation.firstLiveBoundedTargetMap =
            boundedTarget;
        observation.firstLiveServerTimeSeconds =
            m_serverTimelineClock.timeSeconds();
    }

    // Prove the path the authoritative physics actually took, not the
    // conservative sphere that merely broadphases the static object. Sampling
    // a swept segment from the previous fixed-step position prevents a thin
    // obstacle from being skipped between point samples.
    if (m_navigationRuntimeLabStaticGeometryPublished)
    {
        Space::SegmentQuery actualMotion;
        actualMotion.startMapMeters = {
            m_navigationRuntimeLabHasPreviousExactSafetyPosition
                ? m_navigationRuntimeLabPreviousExactSafetyPositionMap.x
                : agentPositionMap.x,
            m_navigationRuntimeLabHasPreviousExactSafetyPosition
                ? m_navigationRuntimeLabPreviousExactSafetyPositionMap.y
                : agentPositionMap.y,
            m_navigationRuntimeLabHasPreviousExactSafetyPosition
                ? m_navigationRuntimeLabPreviousExactSafetyPositionMap.z
                : agentPositionMap.z
        };
        actualMotion.endMapMeters = {
            agentPositionMap.x,
            agentPositionMap.y,
            agentPositionMap.z
        };
        actualMotion.envelope.radiusMeters = shipRadius;
        actualMotion.envelope.additionalClearanceMeters =
            policy.avoidance.staticAdditionalClearanceMeters;
        actualMotion.requireSameRegion = false;
        actualMotion.exactObstaclesOnly = true;

        const Space::SegmentQueryResult actualSafety =
            m_navigationRuntimeLabSpace->querySegment(actualMotion);

        ++m_navigationRuntimeLabObservation.exactStaticMotionSamples;
        if (!actualSafety.traversable)
        {
            auto& observation = m_navigationRuntimeLabObservation;
            observation.exactStaticViolationSeen = true;

            if (!observation.firstExactStaticViolationCaptured)
            {
                observation.firstExactStaticViolationCaptured = true;
                observation.firstExactStaticViolationEntityId =
                    actualSafety.blockingObstacleEntityId;
                observation.firstExactStaticViolationStartMap =
                    m_navigationRuntimeLabHasPreviousExactSafetyPosition
                        ? m_navigationRuntimeLabPreviousExactSafetyPositionMap
                        : agentPositionMap;
                observation.firstExactStaticViolationEndMap =
                    agentPositionMap;
                observation.firstExactStaticViolationSelectedTargetMap =
                    m_navigationRuntimeLabLastPlan.selectedTargetMapMeters;
                observation.firstExactStaticViolationPlannerStatus =
                    static_cast<std::uint8_t>(
                        m_navigationRuntimeLabLastPlan.status
                    );
                observation.firstExactStaticViolationAcceptedSegmentRevision =
                    m_navigationRuntimeLabAcceptedSegment.revision;
                observation.firstExactStaticViolationLastReplanReason =
                    observation.lastReplanReason;
                observation.firstExactStaticViolationAcceptedTargetMap =
                    m_navigationRuntimeLabAcceptedSegment.targetPositionMapMeters;
                observation.firstExactStaticViolationAcceptedTargetVelocityMapMps =
                    m_navigationRuntimeLabAcceptedSegment.
                        targetVelocityMapMetersPerSecond;
                observation.firstExactStaticViolationCurrentVelocityMapMps =
                    agent.velocityMapMetersPerSecond;
                observation.firstExactStaticViolationLastExecutedDemandMapMps2 =
                    observation.lastExecutedLinearDemandMapMps2;
                observation.firstExactStaticViolationPreviousTargetBlocked =
                    observation.acceptedSegmentLastStaticTargetBlocked;
                observation.firstExactStaticViolationPreviousForecastBlocked =
                    observation.acceptedSegmentLastStaticForecastBlocked;
                observation.firstExactStaticViolationPreviousExecutedForecastBlocked =
                    observation.acceptedSegmentLastStaticExecutedForecastBlocked;
                observation.firstExactStaticViolationPreviousBlockingEntityId =
                    observation.acceptedSegmentLastStaticBlockingEntityId;
                observation.firstExactStaticViolationPreviousProbeStartMap =
                    observation.acceptedSegmentLastStaticProbeStartMap;
                observation.firstExactStaticViolationPreviousTargetMap =
                    observation.acceptedSegmentLastStaticTargetMap;
                observation.firstExactStaticViolationPreviousForecastEndMap =
                    observation.acceptedSegmentLastStaticForecastEndMap;
                observation.firstExactStaticViolationPreviousExecutedForecastEndMap =
                    observation.acceptedSegmentLastStaticExecutedForecastEndMap;
                observation.firstExactStaticViolationPreviousVelocityMapMps =
                    observation.acceptedSegmentLastStaticVelocityMapMps;
                observation.firstExactStaticViolationPreviousIdealAccelerationMapMps2 =
                    observation.acceptedSegmentLastStaticIdealAccelerationMapMps2;
                observation.firstExactStaticViolationPreviousExecutedAccelerationMapMps2 =
                    observation.acceptedSegmentLastStaticExecutedAccelerationMapMps2;
                observation.firstExactStaticViolationPreviousForecastSeconds =
                    observation.acceptedSegmentLastStaticForecastSeconds;
                observation.firstExactStaticViolationPreviousProbeTimeSeconds =
                    observation.acceptedSegmentLastStaticProbeTimeSeconds;
            }
        }

        // A tunnel is not accepted by merely putting the hull center through
        // a hole. The actual fixed-step sweep must cross the entry plane with
        // flight-path vector + hull forward axis captured onto the tunnel
        // normal, then remain inside exact geometry until the exit plane.
        if (m_navigationRuntimeLabHasPreviousExactSafetyPosition)
        {
            const glm::dvec3 previous =
                m_navigationRuntimeLabPreviousExactSafetyPositionMap;
            const double dz =
                agentPositionMap.z - previous.z;

            const auto crossingAtZ =
                [&](double planeZ, glm::dvec3& crossing)
                {
                    if (dz <= 1.0e-12 ||
                        previous.z > planeZ ||
                        agentPositionMap.z < planeZ)
                    {
                        return false;
                    }

                    const double t = std::clamp(
                        (planeZ - previous.z) / dz,
                        0.0,
                        1.0
                    );
                    crossing =
                        previous +
                        (agentPositionMap - previous) * t;
                    return true;
                };

            const auto slitMargin =
                [&](const glm::dvec3& crossing)
                {
                    const double xMargin =
                        NavigationRuntimeLabSlitHalfWidthMeters -
                        std::abs(
                            crossing.x -
                            NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.x
                        ) -
                        shipRadius -
                        policy.avoidance.staticAdditionalClearanceMeters;
                    const double yMargin =
                        NavigationRuntimeLabSlitHalfHeightMeters -
                        std::abs(
                            crossing.y -
                            NavigationRuntimeLabSlitPortalCenterVisualLocalMeters.y
                        ) -
                        shipRadius -
                        policy.avoidance.staticAdditionalClearanceMeters;
                    return std::min(xMargin, yMargin);
                };

            glm::dvec3 entryCrossing(0.0);
            if (!m_navigationRuntimeLabObservation.
                    slitEntryPlaneCrossedAligned &&
                crossingAtZ(
                    NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.z,
                    entryCrossing
                ))
            {
                const glm::dvec3 tunnelNormal(0.0, 0.0, 1.0);
                const double speed =
                    glm::length(agent.velocityMapMetersPerSecond);
                const glm::dvec3 lateralVelocity =
                    agent.velocityMapMetersPerSecond -
                    tunnelNormal *
                        glm::dot(
                            agent.velocityMapMetersPerSecond,
                            tunnelNormal
                        );
                const double lateralSpeed =
                    glm::length(lateralVelocity);
                const double velocityAngle =
                    speed <= 0.25
                        ? 0.0
                        : std::acos(
                              std::clamp(
                                  glm::dot(
                                      glm::normalize(
                                          agent.velocityMapMetersPerSecond
                                      ),
                                      tunnelNormal
                                  ),
                                  -1.0,
                                  1.0
                              )
                          );
                const glm::dvec3 forward =
                    glm::normalize(agent.forwardMap);
                const double forwardAngle =
                    std::acos(
                        std::clamp(
                            glm::dot(forward, tunnelNormal),
                            -1.0,
                            1.0
                        )
                    );
                const double crossTrack =
                    glm::length(
                        glm::dvec3(
                            entryCrossing.x -
                                NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.x,
                            entryCrossing.y -
                                NavigationRuntimeLabSlitEntryCenterVisualLocalMeters.y,
                            0.0
                        )
                    );
                const double margin = slitMargin(entryCrossing);

                auto& observation =
                    m_navigationRuntimeLabObservation;
                observation.slitEntryVelocityAngleRad = velocityAngle;
                observation.slitEntryForwardAngleRad = forwardAngle;
                observation.slitEntryLateralSpeedMps = lateralSpeed;
                observation.slitEntryCrossTrackMeters = crossTrack;

                const bool aligned =
                    actualSafety.traversable &&
                    margin >= 0.0 &&
                    velocityAngle <=
                        NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad &&
                    forwardAngle <=
                        NavigationRuntimeLabSlitMaximumEntryForwardAngleRad &&
                    lateralSpeed <=
                        NavigationRuntimeLabSlitMaximumLateralSpeedMps;

                observation.slitEntryPlaneCrossedAligned = aligned;
            }

            glm::dvec3 exitCrossing(0.0);
            if (!m_navigationRuntimeLabObservation.slitTunnelPassed &&
                m_navigationRuntimeLabObservation.
                    slitEntryPlaneCrossedAligned &&
                crossingAtZ(
                    NavigationRuntimeLabSlitExitCenterVisualLocalMeters.z,
                    exitCrossing
                ))
            {
                const double margin = slitMargin(exitCrossing);
                if (actualSafety.traversable && margin >= 0.0)
                {
                    auto& observation =
                        m_navigationRuntimeLabObservation;
                    observation.slitTunnelPassed = true;
                    observation.slitTunnelCrossingMap = exitCrossing;
                    observation.slitTunnelCrossingMarginMeters = margin;
                }
            }
        }

        m_navigationRuntimeLabPreviousExactSafetyPositionMap =
            agentPositionMap;
        m_navigationRuntimeLabHasPreviousExactSafetyPosition = true;
    }

    const glm::dvec3 routeStartMap =
        NavigationRuntimeLabStartVisualLocalMeters;
    const glm::dvec3 routeVectorMap =
        goalPositionMap - routeStartMap;
    const double routeLengthSquared =
        glm::dot(routeVectorMap, routeVectorMap);
    const double goalDistanceMeters =
        glm::length(goalPositionMap - agentPositionMap);

    auto& observation = m_navigationRuntimeLabObservation;
    const bool firstPlanSample = observation.planCount == 0;

    if (firstPlanSample)
    {
        observation.initialGoalDistanceMeters =
            glm::length(goalPositionMap - routeStartMap);
        observation.minimumGoalDistanceMeters =
            goalDistanceMeters;
    }
    else
    {
        observation.minimumGoalDistanceMeters =
            std::min(
                observation.minimumGoalDistanceMeters,
                goalDistanceMeters
            );
    }

    if (routeLengthSquared > 1.0e-12)
    {
        const double routeT = std::clamp(
            glm::dot(
                agentPositionMap - routeStartMap,
                routeVectorMap
            ) / routeLengthSquared,
            0.0,
            1.0
        );
        const glm::dvec3 closestOnStraightRoute =
            routeStartMap + routeVectorMap * routeT;
        observation.maximumStraightLineDeviationMeters =
            std::max(
                observation.maximumStraightLineDeviationMeters,
                glm::length(
                    agentPositionMap - closestOnStraightRoute
                )
            );
    }

    if (haveObservedObstacle)
    {
        const glm::dvec3 obstaclePositionMap =
            navigationBoundary.toNavigation(
                game::navigation::NavigationFrameBoundary::SystemPosition {
                    observedObstacleWorldPosition
                }
            ).meters;
        const double centerDistanceMeters =
            glm::length(agentPositionMap - obstaclePositionMap);
        const double conservativeClearanceMeters =
            centerDistanceMeters -
            shipRadius -
            observedObstacleRadiusMeters;

        if (firstPlanSample ||
            observation.minimumObstacleCenterDistanceMeters <= 0.0)
        {
            observation.minimumObstacleCenterDistanceMeters =
                centerDistanceMeters;
            observation.minimumConservativeClearanceMeters =
                conservativeClearanceMeters;
        }
        else
        {
            observation.minimumObstacleCenterDistanceMeters =
                std::min(
                    observation.minimumObstacleCenterDistanceMeters,
                    centerDistanceMeters
                );
            observation.minimumConservativeClearanceMeters =
                std::min(
                    observation.minimumConservativeClearanceMeters,
                    conservativeClearanceMeters
                );
        }

        if (routeLengthSquared > 1.0e-12)
        {
            const glm::dvec3 routeDirection =
                routeVectorMap /
                std::sqrt(routeLengthSquared);
            const double shipProgress =
                glm::dot(
                    agentPositionMap - routeStartMap,
                    routeDirection
                );
            const double obstacleProgress =
                glm::dot(
                    obstaclePositionMap - routeStartMap,
                    routeDirection
                );
            if (shipProgress > obstacleProgress)
                observation.passedObstaclePlane = true;
        }
    }

    if (goalDistanceMeters <= NavigationRuntimeLabArrivalRadiusMeters)
        observation.reachedGoal = true;

    using Replan =
        game::navigation::NavigationExecutionReplanPolicy;
    using Scheduler =
        game::navigation::NavigationWorkScheduler;
    using PlannerJob =
        game::navigation::NavigationPlannerJob;
    using Follower =
        game::navigation::TrajectoryFollower;
    using AcceptedSegment =
        game::navigation::AcceptedShortSegment;

    constexpr double AcceptedSegmentValiditySeconds = 0.75;
    constexpr double HoldSegmentValiditySeconds = 0.25;

    const double navigationTimeSeconds =
        m_serverTimelineClock.timeSeconds();

    Follower::AgentState followerAgent;
    followerAgent.positionMapMeters = agent.positionMapMeters;
    followerAgent.velocityMapMetersPerSecond =
        agent.velocityMapMetersPerSecond;
    followerAgent.forwardMap = agent.forwardMap;
    followerAgent.rightMap = agent.rightMap;
    followerAgent.upMap = agent.upMap;
    followerAgent.pitchRateRadPerSec = agent.pitchRateRadPerSec;
    followerAgent.yawRateRadPerSec = agent.yawRateRadPerSec;
    followerAgent.rollRateRadPerSec = agent.rollRateRadPerSec;

    const auto followAcceptedSegment =
        [&](const AcceptedSegment& accepted)
        {
            const auto adapted =
                game::diagnostics::
                    NavigationRuntimeLabAcceptedProgramAdapter::adapt(
                        accepted,
                        followerAgent
                    );

            auto result = Follower::follow(
                adapted.program,
                navigationTimeSeconds,
                followerAgent,
                adapted.trackingPolicy
            );

            game::diagnostics::
                NavigationRuntimeLabAcceptedProgramAdapter::
                    preserveLegacyMonitoring(
                        accepted,
                        followerAgent,
                        result
                    );
            return result;
        };

    AcceptedSegment::CapabilitySnapshot currentCapability;
    currentCapability.maxForwardAccelerationMetersPerSec2 =
        agent.linearCapability.maxForwardAccelerationMetersPerSec2;
    currentCapability.maxReverseAccelerationMetersPerSec2 =
        agent.linearCapability.maxReverseAccelerationMetersPerSec2;
    currentCapability.maxLateralAccelerationMetersPerSec2 =
        agent.linearCapability.maxLateralAccelerationMetersPerSec2;
    currentCapability.maxVerticalAccelerationMetersPerSec2 =
        agent.linearCapability.maxVerticalAccelerationMetersPerSec2;
    currentCapability.maxAngularAccelerationRadPerSec2 =
        agent.angularCapability.maxAngularAccelerationRadPerSec2;
    currentCapability.maxAngularSpeedRadPerSec =
        agent.angularCapability.maxAngularSpeedRadPerSec;

    const auto capabilitySnapshotsDiffer =
        [](const AcceptedSegment::CapabilitySnapshot& lhs,
           const AcceptedSegment::CapabilitySnapshot& rhs)
        {
            constexpr double tolerance = 1.0e-9;
            const auto changed =
                [&](double aValue, double bValue)
                {
                    return
                        !std::isfinite(aValue) ||
                        !std::isfinite(bValue) ||
                        std::abs(aValue - bValue) > tolerance;
                };

            return
                changed(
                    lhs.maxForwardAccelerationMetersPerSec2,
                    rhs.maxForwardAccelerationMetersPerSec2
                ) ||
                changed(
                    lhs.maxReverseAccelerationMetersPerSec2,
                    rhs.maxReverseAccelerationMetersPerSec2
                ) ||
                changed(
                    lhs.maxLateralAccelerationMetersPerSec2,
                    rhs.maxLateralAccelerationMetersPerSec2
                ) ||
                changed(
                    lhs.maxVerticalAccelerationMetersPerSec2,
                    rhs.maxVerticalAccelerationMetersPerSec2
                ) ||
                changed(
                    lhs.maxAngularAccelerationRadPerSec2,
                    rhs.maxAngularAccelerationRadPerSec2
                ) ||
                changed(
                    lhs.maxAngularSpeedRadPerSec,
                    rhs.maxAngularSpeedRadPerSec
                );
        };

    if (!m_navigationRuntimeLabSchedulerCapabilityInitialized)
    {
        m_navigationRuntimeLabSchedulerCapability = currentCapability;
        m_navigationRuntimeLabSchedulerCapabilityInitialized = true;
    }
    else if (capabilitySnapshotsDiffer(
                 m_navigationRuntimeLabSchedulerCapability,
                 currentCapability))
    {
        ++m_navigationRuntimeLabCapabilityRevision;
        m_navigationRuntimeLabSchedulerCapability = currentCapability;
    }

    const auto capabilityChanged =
        [&](const AcceptedSegment& accepted)
        {
            return capabilitySnapshotsDiffer(
                accepted.capability,
                currentCapability
            );
        };

    Follower::Result followerResult;
    if (m_navigationRuntimeLabAcceptedSegment.valid)
    {
        followerResult =
            followAcceptedSegment(
                m_navigationRuntimeLabAcceptedSegment
            );
    }

    // Monitoring is allowed every fixed step; planning is not. Re-prove the
    // currently accepted execution against exact physical HitVolumes using
    // both its current geometric target and a short kinematic forecast. This
    // catches inertia/tracking drift before the already-accepted command can
    // carry the hull into static geometry.
    bool staticSafetyInvalidated = false;
    bool staticSafetyTargetBlocked = false;
    bool staticSafetyForecastBlocked = false;
    bool staticSafetyExecutedForecastBlocked = false;
    bool staticSafetyStoppingReserveBlocked = false;
    bool staticSafetyRecoveryRequired = false;
    std::uint32_t staticSafetyBlockingEntityId = 0;
    glm::dvec3 staticSafetyForecastEndMap =
        agent.positionMapMeters;
    glm::dvec3 staticSafetyExecutedForecastEndMap =
        agent.positionMapMeters;
    glm::dvec3 staticSafetyIdealAccelerationMapMps2(0.0);
    glm::dvec3 staticSafetyExecutedAccelerationMapMps2(0.0);
    glm::dvec3 staticSafetyStoppingReserveEndMap =
        agent.positionMapMeters;
    double staticSafetyForecastSeconds = 0.0;
    double staticSafetyStoppingReserveSeconds = 0.0;
    double staticSafetyStoppingReserveDistanceMeters = 0.0;

    if (m_navigationRuntimeLabAcceptedSegment.valid &&
        followerResult.status != Follower::Status::InvalidInput &&
        m_navigationRuntimeLabStaticGeometryPublished)
    {
        const auto exactExecutionSegmentBlocked =
            [&](const glm::dvec3& startMap,
                const glm::dvec3& endMap)
            {
                const glm::dvec3 delta =
                    endMap - startMap;
                if (glm::dot(delta, delta) <= 1.0e-12)
                    return false;

                Space::SegmentQuery query;
                query.startMapMeters = {
                    startMap.x,
                    startMap.y,
                    startMap.z
                };
                query.endMapMeters = {
                    endMap.x,
                    endMap.y,
                    endMap.z
                };
                query.envelope.radiusMeters = shipRadius;
                query.envelope.additionalClearanceMeters =
                    policy.avoidance.staticAdditionalClearanceMeters;
                query.requireSameRegion = false;
                query.exactObstaclesOnly = true;

                const Space::SegmentQueryResult safety =
                    m_navigationRuntimeLabSpace->querySegment(query);

                if (safety.traversable)
                    return false;

                staticSafetyBlockingEntityId =
                    safety.blockingObstacleEntityId;
                return true;
            };

        // The kinematic shape of the reserve is pure value-in/value-out.
        // GameSimulation owns only the authoritative geometry query and state
        // transitions around that probe.
        game::navigation::NavigationExecutionSafetyProbeBuilder::
            StoppingReserveQuery stoppingQuery;
        stoppingQuery.positionMapMeters =
            agent.positionMapMeters;
        stoppingQuery.velocityMapMetersPerSecond =
            agent.velocityMapMetersPerSecond;
        stoppingQuery.controlResponseReserveSeconds =
            navigationControlResponseReserveSeconds;
        stoppingQuery.brakingAccelerationMetersPerSecond2 =
            policy.horizon.maxBrakingAccelerationMetersPerSecond2;

        const auto stoppingProbe =
            game::navigation::NavigationExecutionSafetyProbeBuilder::
                buildStoppingReserve(stoppingQuery);
        if (!stoppingProbe.valid)
            return false;

        staticSafetyStoppingReserveSeconds =
            stoppingProbe.responseSeconds;
        staticSafetyStoppingReserveDistanceMeters =
            stoppingProbe.distanceMeters;
        staticSafetyStoppingReserveEndMap =
            stoppingProbe.endMapMeters;

        if (stoppingProbe.active)
        {
            staticSafetyStoppingReserveBlocked =
                exactExecutionSegmentBlocked(
                    stoppingProbe.startMapMeters,
                    stoppingProbe.endMapMeters
                );

            if (staticSafetyStoppingReserveBlocked)
            {
                staticSafetyInvalidated = true;
                staticSafetyRecoveryRequired = true;
            }
        }

        staticSafetyTargetBlocked =
            exactExecutionSegmentBlocked(
                agent.positionMapMeters,
                m_navigationRuntimeLabAcceptedSegment.
                    targetPositionMapMeters
            );
        // Safety invalidation is monotonic inside one monitor pass. A later
        // independent check may add evidence but must never erase an already
        // proven unsafe stopping reserve.
        staticSafetyInvalidated =
            staticSafetyInvalidated ||
            staticSafetyTargetBlocked;

        if (!staticSafetyInvalidated)
        {
            const double remainingAcceptedSeconds =
                std::max(
                    0.0,
                    m_navigationRuntimeLabAcceptedSegment.
                        validUntilUniverseTimeSeconds -
                    navigationTimeSeconds
                );
            staticSafetyForecastSeconds =
                std::min(2.0, remainingAcceptedSeconds);

            if (staticSafetyForecastSeconds > 1.0e-6)
            {
                staticSafetyIdealAccelerationMapMps2 = glm::dvec3(
                    followerResult.intent.
                        idealLinearAccelerationLocalMps2.x,
                    followerResult.intent.
                        idealLinearAccelerationLocalMps2.y,
                    followerResult.intent.
                        idealLinearAccelerationLocalMps2.z
                );

                game::navigation::
                    NavigationExecutionSafetyProbeBuilder::
                    ConstantAccelerationQuery idealForecastQuery;
                idealForecastQuery.positionMapMeters =
                    agent.positionMapMeters;
                idealForecastQuery.velocityMapMetersPerSecond =
                    agent.velocityMapMetersPerSecond;
                idealForecastQuery.accelerationMapMetersPerSecond2 =
                    staticSafetyIdealAccelerationMapMps2;
                idealForecastQuery.durationSeconds =
                    staticSafetyForecastSeconds;
                idealForecastQuery.maximumDistanceMeters =
                    localHorizonMeters;

                const auto idealForecastProbe =
                    game::navigation::
                        NavigationExecutionSafetyProbeBuilder::
                        buildConstantAccelerationProbe(
                            idealForecastQuery
                        );
                if (!idealForecastProbe.valid)
                    return false;

                staticSafetyForecastEndMap =
                    idealForecastProbe.endMapMeters;
                if (idealForecastProbe.active)
                {
                    staticSafetyForecastBlocked =
                        exactExecutionSegmentBlocked(
                            idealForecastProbe.startMapMeters,
                            idealForecastProbe.endMapMeters
                        );
                    staticSafetyInvalidated =
                        staticSafetyInvalidated ||
                        staticSafetyForecastBlocked;
                }
            }

            // The follower demand is not what authoritative physics necessarily
            // receives on the next fixed step. PilotSkillExecutor can still be
            // reacting, sampling, latency-queuing or filtering the previous
            // command. Prove a conservative continuation of the acceleration
            // that was actually executed on the preceding fixed step. Sample
            // the parabola into short exact-static chords so curvature is not
            // collapsed into one endpoint segment.
            if (!staticSafetyInvalidated &&
                m_navigationRuntimeLabObservation.executionSeen &&
                staticSafetyForecastSeconds > 1.0e-6)
            {
                staticSafetyExecutedAccelerationMapMps2 =
                    m_navigationRuntimeLabObservation.
                        lastExecutedLinearDemandMapMps2;

                game::navigation::
                    NavigationExecutionSafetyProbeBuilder::
                    ConstantAccelerationQuery executedForecastQuery;
                executedForecastQuery.positionMapMeters =
                    agent.positionMapMeters;
                executedForecastQuery.velocityMapMetersPerSecond =
                    agent.velocityMapMetersPerSecond;
                executedForecastQuery.accelerationMapMetersPerSecond2 =
                    staticSafetyExecutedAccelerationMapMps2;
                executedForecastQuery.durationSeconds =
                    staticSafetyForecastSeconds;
                executedForecastQuery.maximumDistanceMeters =
                    localHorizonMeters;

                const auto executedForecast =
                    game::navigation::
                        NavigationExecutionSafetyProbeBuilder::
                        buildSampledConstantAccelerationForecast(
                            executedForecastQuery
                        );
                if (!executedForecast.valid)
                    return false;

                if (executedForecast.active)
                {
                    staticSafetyExecutedForecastEndMap =
                        executedForecast.pointsMapMeters[
                            executedForecast.pointCount - 1
                        ];

                    for (std::size_t sampleIndex = 1;
                         sampleIndex < executedForecast.pointCount;
                         ++sampleIndex)
                    {
                        if (exactExecutionSegmentBlocked(
                                executedForecast.pointsMapMeters[
                                    sampleIndex - 1
                                ],
                                executedForecast.pointsMapMeters[
                                    sampleIndex
                                ]))
                        {
                            staticSafetyExecutedForecastBlocked = true;
                            staticSafetyInvalidated = true;
                            break;
                        }
                    }
                }
            }

        }

        auto& safetyObservation =
            m_navigationRuntimeLabObservation;
        safetyObservation.acceptedSegmentLastStaticTargetBlocked =
            staticSafetyTargetBlocked;
        safetyObservation.acceptedSegmentLastStaticForecastBlocked =
            staticSafetyForecastBlocked;
        safetyObservation.acceptedSegmentLastStaticExecutedForecastBlocked =
            staticSafetyExecutedForecastBlocked;
        safetyObservation.acceptedSegmentLastStoppingReserveBlocked =
            staticSafetyStoppingReserveBlocked;
        safetyObservation.acceptedSegmentLastStoppingReserveEndMap =
            staticSafetyStoppingReserveEndMap;
        safetyObservation.acceptedSegmentLastStoppingReserveSeconds =
            staticSafetyStoppingReserveSeconds;
        safetyObservation.acceptedSegmentLastStoppingReserveDistanceMeters =
            staticSafetyStoppingReserveDistanceMeters;
        safetyObservation.acceptedSegmentLastStaticProbeStartMap =
            agent.positionMapMeters;
        safetyObservation.acceptedSegmentLastStaticTargetMap =
            m_navigationRuntimeLabAcceptedSegment.targetPositionMapMeters;
        safetyObservation.acceptedSegmentLastStaticForecastEndMap =
            staticSafetyForecastEndMap;
        safetyObservation.acceptedSegmentLastStaticExecutedForecastEndMap =
            staticSafetyExecutedForecastEndMap;
        safetyObservation.acceptedSegmentLastStaticVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        safetyObservation.acceptedSegmentLastStaticIdealAccelerationMapMps2 =
            staticSafetyIdealAccelerationMapMps2;
        safetyObservation.acceptedSegmentLastStaticExecutedAccelerationMapMps2 =
            staticSafetyExecutedAccelerationMapMps2;
        safetyObservation.acceptedSegmentLastStaticForecastSeconds =
            staticSafetyForecastSeconds;
        safetyObservation.acceptedSegmentLastStaticProbeTimeSeconds =
            navigationTimeSeconds;

        if (staticSafetyInvalidated)
        {
            ++safetyObservation.
                acceptedSegmentStaticSafetyInvalidationCount;
            safetyObservation.
                acceptedSegmentLastStaticBlockingEntityId =
                    staticSafetyBlockingEntityId;
        }
        else
        {
            safetyObservation.acceptedSegmentLastStaticBlockingEntityId = 0;
        }
    }

    Replan::Query replanQuery;
    replanQuery.mode = Replan::ExecutionMode::Automatic;
    replanQuery.universeTimeSeconds = navigationTimeSeconds;
    replanQuery.acceptedSegmentValid =
        m_navigationRuntimeLabAcceptedSegment.valid &&
        followerResult.status != Follower::Status::InvalidInput;
    replanQuery.acceptedSegmentValidUntilUniverseTimeSeconds =
        m_navigationRuntimeLabAcceptedSegment.
            validUntilUniverseTimeSeconds;
    replanQuery.acceptedSegmentComplete =
        followerResult.status == Follower::Status::Complete;
    replanQuery.trackingErrorExceeded =
        followerResult.trackingErrorExceeded;
    replanQuery.staticSafetyInvalidated =
        staticSafetyInvalidated &&
        !m_navigationRuntimeLabAcceptedSegment.emergency;
    replanQuery.vehicleCapabilityChanged =
        m_navigationRuntimeLabAcceptedSegment.valid &&
        capabilityChanged(m_navigationRuntimeLabAcceptedSegment);
    replanQuery.goalIntentChanged =
        m_navigationRuntimeLabAcceptedSegment.valid &&
        m_navigationRuntimeLabAcceptedSegment.goalRevision !=
            plannerGoal.revision;
    replanQuery.globalRouteValid = true;
    replanQuery.currentTopologyBranchValid = true;

    const Replan::Policy replanPolicy;
    const Replan::Result replan =
        Replan::evaluate(replanPolicy, replanQuery);

    bool plannedThisTick = false;

    // B14 live seam: world/objective/capability revisions are published before
    // any expensive planner job can be dispatched. The scheduler itself owns
    // no planner callback or geometry.
    if (!m_navigationRuntimeLabWorkScheduler.setCurrentWorldRevision(
            m_navigationRuntimeLabSourceRevision))
    {
        return false;
    }

    Scheduler::ActorRevisionStamp schedulerActorRevision;
    schedulerActorRevision.actorId = id.value;
    schedulerActorRevision.objectiveRevision = plannerGoal.revision;
    schedulerActorRevision.capabilityRevision =
        m_navigationRuntimeLabCapabilityRevision;
    schedulerActorRevision.routeRevision = 0;

    if (!m_navigationRuntimeLabWorkScheduler.publishActorRevision(
            schedulerActorRevision))
    {
        return false;
    }

    if (replan.scope != Replan::Scope::None)
    {
        const auto schedulerPriority =
            [](Replan::Reason reason)
            {
                switch (reason)
                {
                case Replan::Reason::TrackingErrorExceeded:
                case Replan::Reason::StaticSafetyInvalidated:
                case Replan::Reason::DynamicHazardInvalidated:
                case Replan::Reason::VehicleCapabilityChanged:
                    return PlannerJob::Priority::Urgent;

                case Replan::Reason::ManualPeriodicRefresh:
                    return PlannerJob::Priority::Background;

                default:
                    return PlannerJob::Priority::Normal;
                }
            };

        const auto schedulerTrigger =
            [](Replan::Reason reason)
            {
                switch (reason)
                {
                case Replan::Reason::NoAcceptedSegment:
                    return PlannerJob::Trigger::MissingProgram;
                case Replan::Reason::SegmentExpired:
                    return PlannerJob::Trigger::ProgramExpired;
                case Replan::Reason::SegmentCompleted:
                    return PlannerJob::Trigger::ProgramCompleted;
                case Replan::Reason::TrackingErrorExceeded:
                    return PlannerJob::Trigger::TrackingErrorExceeded;
                case Replan::Reason::StaticSafetyInvalidated:
                    return PlannerJob::Trigger::StaticSafetyInvalidated;
                case Replan::Reason::DynamicHazardInvalidated:
                    return PlannerJob::Trigger::DynamicHazardInvalidated;
                case Replan::Reason::VehicleCapabilityChanged:
                    return PlannerJob::Trigger::CapabilityChanged;
                case Replan::Reason::GoalIntentChanged:
                    return PlannerJob::Trigger::ObjectiveChanged;
                case Replan::Reason::TopologyBranchInvalidated:
                    return PlannerJob::Trigger::TopologyInvalidated;
                case Replan::Reason::ManualPeriodicRefresh:
                case Replan::Reason::ManualCorridorExit:
                    return PlannerJob::Trigger::ManualRefresh;
                default:
                    return PlannerJob::Trigger::AdvisoryPrewarm;
                }
            };

        PlannerJob schedulerJob;
        schedulerJob.actorId = id.value;
        schedulerJob.jobRevision =
            m_navigationRuntimeLabNextPlannerJobRevision++;
        schedulerJob.objectiveRevision = plannerGoal.revision;
        schedulerJob.worldRevision =
            m_navigationRuntimeLabSourceRevision;
        schedulerJob.capabilityRevision =
            m_navigationRuntimeLabCapabilityRevision;
        schedulerJob.routeRevision = 0;
        schedulerJob.priority = schedulerPriority(replan.reason);
        schedulerJob.scope =
            replan.scope == Replan::Scope::FullRoute
                ? PlannerJob::Scope::FullRoute
                : PlannerJob::Scope::LocalHorizon;
        schedulerJob.trigger = schedulerTrigger(replan.reason);
        schedulerJob.guidanceOnly = replan.guidanceOnly;
        schedulerJob.estimatedCostUnits =
            schedulerJob.scope == PlannerJob::Scope::FullRoute
                ? 4u
                : 1u;

        const auto enqueueResult =
            m_navigationRuntimeLabWorkScheduler.enqueue(
                schedulerJob,
                m_navigationRuntimeLabSourceRevision
            );

        auto& schedulerObservation =
            m_navigationRuntimeLabObservation;

        switch (enqueueResult.status)
        {
        case Scheduler::EnqueueStatus::Accepted:
            ++schedulerObservation.schedulerEnqueueAcceptedCount;
            break;
        case Scheduler::EnqueueStatus::Replaced:
            ++schedulerObservation.schedulerEnqueueReplacedCount;
            break;
        case Scheduler::EnqueueStatus::Duplicate:
            ++schedulerObservation.schedulerEnqueueDuplicateCount;
            break;
        case Scheduler::EnqueueStatus::Stale:
            ++schedulerObservation.schedulerEnqueueStaleCount;
            break;
        case Scheduler::EnqueueStatus::InvalidInput:
        case Scheduler::EnqueueStatus::CapacityExceeded:
            return false;
        }

        const auto queuedSchedulerStats =
            m_navigationRuntimeLabWorkScheduler.stats();
        schedulerObservation.schedulerMaximumPendingJobs =
            std::max(
                schedulerObservation.schedulerMaximumPendingJobs,
                queuedSchedulerStats.pending
            );
        schedulerObservation.schedulerMaximumInFlightJobs =
            std::max(
                schedulerObservation.schedulerMaximumInFlightJobs,
                queuedSchedulerStats.inFlight
            );

        Scheduler::DispatchBudget schedulerBudget;
        schedulerBudget.maxJobs = 1;
        schedulerBudget.maxCostUnits = 4;

        const auto dispatchStart =
            std::chrono::steady_clock::now();
        const auto dispatched =
            m_navigationRuntimeLabWorkScheduler.dispatchSlice(
                m_navigationRuntimeLabSourceRevision,
                schedulerBudget
            );
        const auto dispatchEnd =
            std::chrono::steady_clock::now();

        const double dispatchMicroseconds =
            std::chrono::duration<double, std::micro>(
                dispatchEnd - dispatchStart
            ).count();
        schedulerObservation.schedulerDispatchTotalMicroseconds +=
            dispatchMicroseconds;
        schedulerObservation.schedulerDispatchMaximumMicroseconds =
            std::max(
                schedulerObservation.schedulerDispatchMaximumMicroseconds,
                dispatchMicroseconds
            );

        const auto schedulerStats =
            m_navigationRuntimeLabWorkScheduler.stats();
        schedulerObservation.schedulerMaximumPendingJobs =
            std::max(
                schedulerObservation.schedulerMaximumPendingJobs,
                schedulerStats.pending
            );
        schedulerObservation.schedulerMaximumInFlightJobs =
            std::max(
                schedulerObservation.schedulerMaximumInFlightJobs,
                schedulerStats.inFlight
            );

        if (dispatched.invalidInput)
            return false;

        if (dispatched.count == 1)
        {
            const auto& dispatchedItem = dispatched.items[0];
            if (dispatchedItem.job.actorId != id.value)
                return false;

            ++schedulerObservation.schedulerDispatchCount;

            const Planner::StaticQueries staticQueries(
                *m_navigationRuntimeLabSpace
            );

            const auto plannerStart =
                std::chrono::steady_clock::now();
            const Planner::Result planned =
                Planner::plan(
                    agent,
                    plannerGoal,
                    dynamicCandidates,
                    0.0,
                    staticQueries,
                    policy
                );
            const auto plannerEnd =
                std::chrono::steady_clock::now();

            const double plannerMicroseconds =
                std::chrono::duration<double, std::micro>(
                    plannerEnd - plannerStart
                ).count();
            schedulerObservation.schedulerPlannerTotalMicroseconds +=
                plannerMicroseconds;
            schedulerObservation.schedulerPlannerMaximumMicroseconds =
                std::max(
                    schedulerObservation.schedulerPlannerMaximumMicroseconds,
                    plannerMicroseconds
                );

            ++schedulerObservation.planCount;

            const Scheduler::CompletionStatus completion =
                m_navigationRuntimeLabWorkScheduler.complete(
                    dispatchedItem.ticket
                );

            if (completion ==
                Scheduler::CompletionStatus::CompletedStale)
            {
                ++schedulerObservation.schedulerCompletedStaleCount;
            }
            else if (completion ==
                     Scheduler::CompletionStatus::CompletedCurrent)
            {
                ++schedulerObservation.schedulerCompletedCurrentCount;

                // Commit only after B14 confirms that the completed planner
                // result still matches current world/objective/capability
                // revisions.
                m_navigationRuntimeLabLastPlan = planned;
                ++schedulerObservation.acceptedSegmentReplanCount;
                schedulerObservation.lastReplanReason =
                    static_cast<std::uint8_t>(replan.reason);
                plannedThisTick = true;

                if (m_navigationRuntimeLabLastPlan.status ==
                    Planner::Status::InvalidInput)
                {
                    return false;
                }

        AcceptedSegment accepted;
        accepted.valid = true;
        accepted.revision =
            m_navigationRuntimeLabNextSegmentRevision++;
        accepted.goalRevision = plannerGoal.revision;
        accepted.acceptedAtUniverseTimeSeconds =
            navigationTimeSeconds;

        const bool provisionalHold =
            m_navigationRuntimeLabLastPlan.status ==
                Planner::Status::ConflictHold ||
            m_navigationRuntimeLabLastPlan.status ==
                Planner::Status::StaticHold ||
            m_navigationRuntimeLabLastPlan.status ==
                Planner::Status::StaleHold;

        accepted.validUntilUniverseTimeSeconds =
            navigationTimeSeconds +
            (provisionalHold
                ? HoldSegmentValiditySeconds
                : AcceptedSegmentValiditySeconds);

        accepted.mapRevision =
            m_navigationRuntimeLabLastPlan.mapRevision;
        accepted.mapSourceRevision =
            m_navigationRuntimeLabLastPlan.mapSourceRevision;
        accepted.spaceRevision =
            m_navigationRuntimeLabLastPlan.spaceRevision;
        accepted.spaceSourceRevision =
            m_navigationRuntimeLabLastPlan.spaceSourceRevision;

        accepted.startPositionMapMeters =
            agent.positionMapMeters;
        accepted.targetPositionMapMeters =
            provisionalHold
                ? agent.positionMapMeters
                : m_navigationRuntimeLabLastPlan.
                    selectedTargetMapMeters;
        accepted.targetVelocityMapMetersPerSecond =
            provisionalHold
                ? glm::dvec3(0.0)
                : m_navigationRuntimeLabLastPlan.
                    desiredVelocityMapMetersPerSecond;

        accepted.linearMode =
            m_navigationRuntimeLabLastPlan.
                movingPassageAuthorityUsed
                ? AcceptedSegment::LinearMode::FixedAcceleration
                : AcceptedSegment::LinearMode::VelocityTracking;
        accepted.fixedLinearAccelerationMapMps2 =
            m_navigationRuntimeLabLastPlan.
                movingPassageAuthorityUsed
                ? m_navigationRuntimeLabLastPlan.
                    movingPassageInitialAccelerationMapMps2
                : glm::dvec3(0.0);
        accepted.velocityResponsePerSecond =
            plannerGoal.velocityResponsePerSecond;

        // Preserve the selected maneuver exactly. Route context is not
        // allowed to invent an attitude requirement at the ACCEPT boundary.
        accepted.alignForward =
            m_navigationRuntimeLabLastPlan.
                selectedManeuverRequiresForwardAlignment;
        accepted.desiredForwardMap =
            accepted.alignForward
                ? m_navigationRuntimeLabLastPlan.
                    selectedManeuverForwardMap
                : agent.forwardMap;
        accepted.angularDampingPerSecond =
            plannerGoal.angularDampingPerSecond;
        accepted.orientationResponsePerSecond2 =
            policy.portalTraversal.orientationResponsePerSecond2;
        accepted.maximumAngularAccelerationRadPerSec2 =
            agent.angularCapability.
                maxAngularAccelerationRadPerSec2;

        accepted.completionTriggersReplan =
            !provisionalHold;
        accepted.completionRadiusMeters =
            std::max(1.0, plannerGoal.arrivalRadiusMeters);
        accepted.trackingEnvelopeRadiusMeters =
            std::max(
                accepted.completionRadiusMeters,
                shipRadius +
                    policy.horizon.safetyMarginMeters
            );

        accepted.emergency =
            m_navigationRuntimeLabLastPlan.intent.emergency;
        accepted.hazardUrgency01 =
            m_navigationRuntimeLabLastPlan.intent.hazardUrgency01;

        accepted.capability.maxForwardAccelerationMetersPerSec2 =
            agent.linearCapability.
                maxForwardAccelerationMetersPerSec2;
        accepted.capability.maxReverseAccelerationMetersPerSec2 =
            agent.linearCapability.
                maxReverseAccelerationMetersPerSec2;
        accepted.capability.maxLateralAccelerationMetersPerSec2 =
            agent.linearCapability.
                maxLateralAccelerationMetersPerSec2;
        accepted.capability.maxVerticalAccelerationMetersPerSec2 =
            agent.linearCapability.
                maxVerticalAccelerationMetersPerSec2;
        accepted.capability.maxAngularAccelerationRadPerSec2 =
            agent.angularCapability.
                maxAngularAccelerationRadPerSec2;
        accepted.capability.maxAngularSpeedRadPerSec =
            agent.angularCapability.maxAngularSpeedRadPerSec;

        if (staticSafetyRecoveryRequired)
        {
            const double speed =
                glm::length(agent.velocityMapMetersPerSecond);
            if (speed > 1.0e-6)
            {
                const double brakingAcceleration =
                    std::max(
                        0.5,
                        policy.horizon.
                            maxBrakingAccelerationMetersPerSecond2
                    );

                accepted.linearMode =
                    AcceptedSegment::LinearMode::FixedAcceleration;
                accepted.fixedLinearAccelerationMapMps2 =
                    -agent.velocityMapMetersPerSecond /
                    speed *
                    brakingAcceleration;
                accepted.targetVelocityMapMetersPerSecond =
                    glm::dvec3(0.0);
                accepted.targetPositionMapMeters =
                    staticSafetyStoppingReserveEndMap;
                accepted.completionTriggersReplan = false;
                accepted.validUntilUniverseTimeSeconds =
                    navigationTimeSeconds + 0.25;
                accepted.alignForward = false;
                accepted.emergency = true;
                accepted.hazardUrgency01 = 1.0;

                ++m_navigationRuntimeLabObservation.
                    acceptedSegmentEmergencyRecoveryCount;
            }
        }

        m_navigationRuntimeLabAcceptedSegment = accepted;
        m_navigationRuntimeLabObservation.
            acceptedSegmentEmergencyRecoveryActive =
                accepted.emergency;

        followerResult =
            followAcceptedSegment(
                m_navigationRuntimeLabAcceptedSegment
            );

            }
            else
            {
                return false;
            }
        }
    }

    if (!m_navigationRuntimeLabAcceptedSegment.valid ||
        followerResult.status == Follower::Status::InvalidInput)
    {
        return false;
    }

    if (!plannedThisTick &&
        !replan.continueAcceptedAutomaticExecution)
    {
        return false;
    }

    ++m_navigationRuntimeLabObservation.acceptedSegmentFollowCount;
    m_navigationRuntimeLabObservation.acceptedSegmentActive = true;
    m_navigationRuntimeLabObservation.acceptedSegmentEmergencyRecoveryActive =
        m_navigationRuntimeLabAcceptedSegment.emergency;
    m_navigationRuntimeLabObservation.acceptedSegmentRevision =
        m_navigationRuntimeLabAcceptedSegment.revision;
    m_navigationRuntimeLabObservation.lastIntentRevision =
        followerResult.intent.revision;
    m_navigationRuntimeLabObservation.lastPlannerStatus =
        static_cast<std::uint8_t>(
            m_navigationRuntimeLabLastPlan.status
        );

    if (m_navigationRuntimeLabLastPlan.portalTraversalActive)
    {
        auto& portalObservation =
            m_navigationRuntimeLabObservation;

        if (m_navigationRuntimeLabLastPlan.activePortalId ==
            NavigationRuntimeLabSlitEntryPortalId)
        {
            portalObservation.slitEntryVelocityAngleRad =
                m_navigationRuntimeLabLastPlan.portalVelocityAngleRad;
            portalObservation.slitEntryForwardAngleRad =
                m_navigationRuntimeLabLastPlan.portalForwardAngleRad;
            portalObservation.slitEntryLateralSpeedMps =
                m_navigationRuntimeLabLastPlan.portalLateralSpeedMps;
            portalObservation.slitEntryCrossTrackMeters =
                m_navigationRuntimeLabLastPlan.portalCrossTrackMeters;

            portalObservation.slitEntryVelocityAlignedSeen =
                portalObservation.slitEntryVelocityAlignedSeen ||
                m_navigationRuntimeLabLastPlan.portalVelocityAligned;
            portalObservation.slitEntryForwardAlignedSeen =
                portalObservation.slitEntryForwardAlignedSeen ||
                m_navigationRuntimeLabLastPlan.portalForwardAligned;
            portalObservation.slitEntryCaptureSeen =
                portalObservation.slitEntryCaptureSeen ||
                m_navigationRuntimeLabLastPlan.portalCaptureReady;
        }
    }

    if (m_navigationRuntimeLabLastPlan.usedPortalWaypoint &&
        !m_navigationRuntimeLabLastPlan.staticPortalPath.empty())
    {
        const auto frontPortal =
            m_navigationRuntimeLabLastPlan.staticPortalPath.front();
        if (frontPortal == NavigationRuntimeLabSlitEntryPortalId ||
            frontPortal == NavigationRuntimeLabSlitExitPortalId)
        {
            m_navigationRuntimeLabObservation.slitPortalWaypointSeen = true;
        }
    }

    const bool expectedMovingGapPairSelected =
        movingGapUpperEntityId != 0 &&
        movingGapLowerEntityId != 0 &&
        ((m_navigationRuntimeLabLastPlan.movingPrimaryObstacleEntityId ==
              movingGapUpperEntityId &&
          m_navigationRuntimeLabLastPlan.movingSecondaryObstacleEntityId ==
              movingGapLowerEntityId) ||
         (m_navigationRuntimeLabLastPlan.movingPrimaryObstacleEntityId ==
              movingGapLowerEntityId &&
          m_navigationRuntimeLabLastPlan.movingSecondaryObstacleEntityId ==
              movingGapUpperEntityId));

    const bool movingPairIsNominalConflict =
        (movingGapUpperEntityId != 0 &&
         (m_navigationRuntimeLabLastPlan.nominalPrimaryConflictEntityId ==
              movingGapUpperEntityId ||
          m_navigationRuntimeLabLastPlan.primaryConflictEntityId ==
              movingGapUpperEntityId)) ||
        (movingGapLowerEntityId != 0 &&
         (m_navigationRuntimeLabLastPlan.nominalPrimaryConflictEntityId ==
              movingGapLowerEntityId ||
          m_navigationRuntimeLabLastPlan.primaryConflictEntityId ==
              movingGapLowerEntityId));

    const bool visibleHorizonBypassActive =
        movingPairIsNominalConflict &&
        m_navigationRuntimeLabLastPlan.status ==
            Planner::Status::AdjustedClear &&
        m_navigationRuntimeLabLastPlan.adjustedTarget;

    auto& visibleHorizonObservation =
        m_navigationRuntimeLabObservation;
    visibleHorizonObservation.visibleHorizonBypassActive =
        visibleHorizonBypassActive;
    visibleHorizonObservation.visibleHorizonBypassSeen =
        visibleHorizonObservation.visibleHorizonBypassSeen ||
        visibleHorizonBypassActive;
    visibleHorizonObservation.maximumVisibleHorizonOffsetMeters =
        std::max(
            visibleHorizonObservation.maximumVisibleHorizonOffsetMeters,
            m_navigationRuntimeLabLastPlan.
                localBypassLateralOffsetMeters
        );

    if (visibleHorizonBypassActive &&
        !visibleHorizonObservation.firstVisibleHorizonBypassCaptured)
    {
        visibleHorizonObservation.firstVisibleHorizonBypassCaptured = true;
        visibleHorizonObservation.firstVisibleHorizonBypassTimeSeconds =
            navigationTimeSeconds;
        visibleHorizonObservation.firstVisibleHorizonBypassSegmentRevision =
            m_navigationRuntimeLabAcceptedSegment.revision;
        visibleHorizonObservation.firstVisibleHorizonBypassReplanReason =
            static_cast<std::uint8_t>(replan.reason);
        visibleHorizonObservation.firstVisibleHorizonBypassOffsetMeters =
            m_navigationRuntimeLabLastPlan.
                localBypassLateralOffsetMeters;
        visibleHorizonObservation.firstVisibleHorizonBypassAgentPositionMap =
            agent.positionMapMeters;
        visibleHorizonObservation.firstVisibleHorizonBypassAgentVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        visibleHorizonObservation.firstVisibleHorizonBypassSelectedTargetMap =
            m_navigationRuntimeLabLastPlan.selectedTargetMapMeters;
        visibleHorizonObservation.firstVisibleHorizonBypassDesiredVelocityMapMps =
            m_navigationRuntimeLabLastPlan.
                desiredVelocityMapMetersPerSecond;
        visibleHorizonObservation.firstVisibleHorizonBypassAcceptedAlignForward =
            m_navigationRuntimeLabAcceptedSegment.alignForward;
        visibleHorizonObservation.firstVisibleHorizonBypassAcceptedForwardMap =
            m_navigationRuntimeLabAcceptedSegment.desiredForwardMap;
        visibleHorizonObservation.firstVisibleHorizonBypassIdealAccelerationMapMps2 =
            followerResult.intent.idealLinearAccelerationLocalMps2;
    }

    if (visibleHorizonObservation.visibleHorizonBypassSeen &&
        !m_navigationRuntimeLabLastPlan.adjustedTarget &&
        m_navigationRuntimeLabLastPlan.safeProgressTargetDemonstrated &&
        m_navigationRuntimeLabLastPlan.status !=
            Planner::Status::ConflictHold &&
        m_navigationRuntimeLabLastPlan.status !=
            Planner::Status::StaticHold &&
        m_navigationRuntimeLabLastPlan.status !=
            Planner::Status::StaleHold)
    {
        visibleHorizonObservation.visibleHorizonDirectRecoveredSeen = true;
    }

    m_navigationRuntimeLabObservation.movingPrecisionAttemptedSeen =
        m_navigationRuntimeLabObservation.movingPrecisionAttemptedSeen ||
        m_navigationRuntimeLabLastPlan.movingPrecisionAttempted;

    if (m_navigationRuntimeLabLastPlan.movingPassagesEvaluated > 0)
    {
        auto& movingObservation =
            m_navigationRuntimeLabObservation;
        movingObservation.movingPassageLastEvaluatorStatus =
            static_cast<std::uint8_t>(
                m_navigationRuntimeLabLastPlan.
                    movingPassageLastEvaluatorStatus
            );
        movingObservation.movingPassageRequiredPeakForwardAccelerationMps2 =
            m_navigationRuntimeLabLastPlan.
                movingPassageRequiredPeakForwardAccelerationMps2;
        movingObservation.movingPassageRequiredPeakReverseAccelerationMps2 =
            m_navigationRuntimeLabLastPlan.
                movingPassageRequiredPeakReverseAccelerationMps2;
        movingObservation.movingPassageRequiredPeakLateralAccelerationMps2 =
            m_navigationRuntimeLabLastPlan.
                movingPassageRequiredPeakLateralAccelerationMps2;
        movingObservation.movingPassageRequiredPeakVerticalAccelerationMps2 =
            m_navigationRuntimeLabLastPlan.
                movingPassageRequiredPeakVerticalAccelerationMps2;
        movingObservation.movingPassageMinimumSampleClearanceMeters =
            m_navigationRuntimeLabLastPlan.
                movingPassageMinimumSampleClearanceMeters;
        movingObservation.movingPassageMinimumContinuousClearanceMeters =
            m_navigationRuntimeLabLastPlan.
                movingPassageMinimumContinuousClearanceMeters;
    }

    m_navigationRuntimeLabObservation.movingPassageFeasibleSeen =
        m_navigationRuntimeLabObservation.movingPassageFeasibleSeen ||
        (expectedMovingGapPairSelected &&
         m_navigationRuntimeLabLastPlan.movingPassageFeasible);
    m_navigationRuntimeLabObservation.movingPassageStaticSafeSeen =
        m_navigationRuntimeLabObservation.movingPassageStaticSafeSeen ||
        (expectedMovingGapPairSelected &&
         m_navigationRuntimeLabLastPlan.movingPassageStaticSafe);

    const bool movingPassageAuthorityActive =
        expectedMovingGapPairSelected &&
        m_navigationRuntimeLabLastPlan.movingPassageAuthorityUsed &&
        m_navigationRuntimeLabLastPlan.status ==
            Planner::Status::MovingPassageClear;

    m_navigationRuntimeLabObservation.movingPassageAuthorityActive =
        movingPassageAuthorityActive;
    m_navigationRuntimeLabObservation.movingPassageAuthoritySeen =
        m_navigationRuntimeLabObservation.movingPassageAuthoritySeen ||
        movingPassageAuthorityActive;

    if ((m_navigationRuntimeLabObservation.visibleHorizonBypassSeen ||
         m_navigationRuntimeLabObservation.movingPassageAuthoritySeen) &&
        routeLengthSquared > 1.0e-12)
    {
        const glm::dvec3 routeDirection =
            routeVectorMap / std::sqrt(routeLengthSquared);
        const double shipProgress =
            glm::dot(
                agentPositionMap - routeStartMap,
                routeDirection
            );
        const double movingGapProgress =
            glm::dot(
                m_navigationRuntimeLabObservation.movingGapCurrentCenterMap -
                    routeStartMap,
                routeDirection
            );
        if (shipProgress > movingGapProgress)
        {
            m_navigationRuntimeLabObservation.movingGapPlanePassed = true;
        }
    }

    m_navigationRuntimeLabObservation.adjustedTargetSeen =
        m_navigationRuntimeLabObservation.adjustedTargetSeen ||
        m_navigationRuntimeLabLastPlan.adjustedTarget;
    m_navigationRuntimeLabObservation.conflictHoldSeen =
        m_navigationRuntimeLabObservation.conflictHoldSeen ||
        m_navigationRuntimeLabLastPlan.status ==
            Planner::Status::ConflictHold;
    m_navigationRuntimeLabObservation.exactStaticQuerySeen =
        m_navigationRuntimeLabObservation.exactStaticQuerySeen ||
        m_navigationRuntimeLabLastPlan.staticObstaclesExamined > 0;
    m_navigationRuntimeLabObservation.nominalStaticBlockSeen =
        m_navigationRuntimeLabObservation.nominalStaticBlockSeen ||
        m_navigationRuntimeLabLastPlan.nominalStaticBlocked;
    m_navigationRuntimeLabObservation.maximumExactStaticObstaclesExamined =
        std::max(
            m_navigationRuntimeLabObservation.
                maximumExactStaticObstaclesExamined,
            m_navigationRuntimeLabLastPlan.staticObstaclesExamined
        );

    if (m_navigationRuntimeLabObservation.obstacleEntityId != 0 &&
        m_navigationRuntimeLabLastPlan.nominalStaticObstacleEntityId ==
            m_navigationRuntimeLabObservation.obstacleEntityId)
    {
        m_navigationRuntimeLabObservation.obstacleExactStaticBlockSeen = true;
    }

    if (m_navigationRuntimeLabObservation.obstacleEntityId != 0 &&
        (m_navigationRuntimeLabLastPlan.primaryConflictEntityId ==
             m_navigationRuntimeLabObservation.obstacleEntityId ||
         m_navigationRuntimeLabLastPlan.nominalPrimaryConflictEntityId ==
             m_navigationRuntimeLabObservation.obstacleEntityId))
    {
        m_navigationRuntimeLabObservation.obstaclePrimaryConflictSeen = true;
    }

    outIntent =
        navigationBoundary.toSystemControlIntent(
            followerResult.intent
        );
    return true;
}
