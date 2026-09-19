#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/NavigationControlIntent.h"
#include "src/world/navigation/local/LocalAvoidancePlanner.h"
#include "src/world/navigation/map/NavigationMap.h"
#include "src/world/navigation/space/NavigationStaticQueryApi.h"
#include "src/world/navigation/trajectory/BoundedGapCandidateBuilder.h"
#include "src/world/navigation/trajectory/MovingGapPredictor.h"
#include "src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.h"

namespace game::navigation
{

// Live composition seam between accepted NavigationWorld products and the
// accepted pilot/runtime-control bridge.
//
// This class does not own world geometry, spatial indexes, vehicle physics or
// replication. It consumes one already-published dynamic query plus one
// read-only static query capability, selects the next bounded target, and converts that
// target into the ideal acceleration intent consumed by PilotSkillExecutor.
class NavigationRuntimePlanner final
{
public:
    using Map = world::navigation::NavigationMap;
    using StaticQueries = world::navigation::NavigationStaticQueryApi;
    using Horizon = world::navigation::LocalHorizonPlanner;
    using Avoidance = world::navigation::LocalAvoidancePlanner;
    using GapBuilder = world::navigation::BoundedGapCandidateBuilder;
    using GapPredictor = world::navigation::MovingGapPredictor;
    using MovingPassage = world::navigation::MovingPassageTrajectoryEvaluator;

    struct AgentState
    {
        Map::EntityId entityId = 0;

        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMetersPerSecond {0.0};
        glm::dvec3 accelerationMapMetersPerSecond2 {0.0};
        double radiusMeters = 0.0;

        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;

        // Precision moving-passage proxy and real vehicle authority. The
        // broadphase radius remains conservative; this OBB-like proxy is used
        // only by the bounded precision evaluator.
        glm::dvec3 hullHalfExtentsBodyMeters {0.0};
        MovingPassage::LinearCapability linearCapability {};
        MovingPassage::AngularCapability angularCapability {};
        MovingPassage::ControlMode controlMode =
            MovingPassage::ControlMode::Newtonian;
        double assistedMaxVelocityToForwardAngleRad =
            3.141592653589793238462643383279502884;
    };

    struct Goal
    {
        std::uint64_t revision = 1;

        glm::dvec3 targetPositionMapMeters {0.0};
        glm::dvec3 targetVelocityMapMetersPerSecond {0.0};
        glm::dvec3 targetAccelerationMapMetersPerSecond2 {0.0};

        double maximumTargetSpeedMps = 20.0;
        double velocityResponsePerSecond = 0.75;
        double angularDampingPerSecond = 2.0;
        double arrivalRadiusMeters = 1.0;

        bool emergency = false;
        double hazardUrgency01 = 0.0;
    };

    struct MovingPassagePolicy
    {
        // Enables bounded moving-gap / moving-passage precision evaluation.
        // This may be used for diagnostics without changing steering authority.
        bool enabled = false;

        // Separate opt-in authority gate. Even when precision evaluation is
        // enabled, steering changes only when this flag is true AND both the
        // dynamic moving-passage proof and the same-trajectory exact-static
        // proof are green.
        bool allowSteeringAuthority = false;

        // The gap-builder remains bounded to <=8 candidates by its own hard
        // contract. These policies define the small local search window and the
        // time-varying continuous proof.
        GapBuilder::Policy candidates {};
        GapPredictor::Policy prediction {};

        double durationSeconds = 3.0;
        double hullAdditionalClearanceMeters = 0.0;
        double maximumAcceptedGapTravelAlignment = 0.5;

        // MovingPassageTrajectoryEvaluator currently proves attitude endpoints,
        // not arbitrary initial body angular momentum. Until that contract is
        // extended, only near-stabilized actors may enter this precision path.
        double maximumInitialAngularRateRadPerSec = 0.05;
    };

    struct PortalTraversalPolicy
    {
        bool enabled = true;

        // Translational capture pulls the vehicle onto the portal centerline
        // before entry. Angular capture rotates the hull longitudinal axis onto
        // the oriented portal normal. Vehicle capability remains downstream
        // authority; these are controller response gains, not hard-coded craft
        // acceleration limits.
        double capturePositionResponsePerSecond = 0.25;
        double orientationResponsePerSecond2 = 4.0;
        double minimumSpeedForDirectionMps = 0.25;
    };

    struct Policy
    {
        StaticQueries::CorridorCostPolicy corridor {};
        Horizon::Policy horizon {};
        Avoidance::Policy avoidance {};
        MovingPassagePolicy movingPassage {};
        PortalTraversalPolicy portalTraversal {};
    };

    enum class Status : std::uint8_t
    {
        // Preserve the previously accepted numeric values; diagnostics may
        // persist these statuses outside the planner.
        NominalClear = 0,
        AdjustedClear = 1,
        ConflictHold = 2,
        StaleHold = 3,
        StaticHold = 4,
        InvalidInput = 5,
        MovingPassageClear = 6,
        PortalCapture = 7,
        PortalTransit = 8
    };

    struct Result
    {
        Status status = Status::InvalidInput;

        // Planner-space product. This is a distinct NavLocal-only type and
        // cannot enter PilotSkillExecutor without NavigationFrameBoundary.
        NavigationLocalControlIntent intent {};

        bool safeProgressTargetDemonstrated = false;
        bool usedPortalWaypoint = false;
        bool adjustedTarget = false;
        double selectedVisibilityDeflectionRadians = 0.0;

        // LocalAvoidance tested the complete ordinary progress-preserving fan
        // and found no safe target. Higher game/control logic must now consider
        // recovery/backtrack/flip-and-burn/emergency candidates rather than
        // interpreting the provisional hold intent as a permanent decision.
        bool ordinaryVisibilitySearchExhausted = false;

        glm::dvec3 coarseWaypointMapMeters {0.0};
        glm::dvec3 selectedTargetMapMeters {0.0};
        glm::dvec3 desiredVelocityMapMetersPerSecond {0.0};

        // Attitude semantics belong to the selected CURRENT maneuver, not to
        // route context. A future oriented portal may exist on the route while
        // a local visibility bypass is active; that must not silently force
        // the accepted segment to face the future portal.
        bool selectedManeuverRequiresForwardAlignment = false;
        glm::dvec3 selectedManeuverForwardMap {0.0, 0.0, -1.0};

        StaticQueries::Revision spaceRevision = 0;
        StaticQueries::Revision spaceSourceRevision = 0;
        Map::Revision mapRevision = 0;
        Map::Revision mapSourceRevision = 0;

        std::vector<StaticQueries::RegionId> staticRegionPath;
        std::vector<StaticQueries::PortalId> staticPortalPath;
        std::vector<StaticQueries::Vec3d> staticPortalCentersMapMeters;
        std::vector<StaticQueries::PortalTraversal> staticPortalTraversals;

        // First finite-depth/oriented portal traversal, when present.
        bool portalTraversalActive = false;
        StaticQueries::PortalId activePortalId = 0;
        glm::dvec3 portalNormalMap {0.0};
        glm::dvec3 portalApproachPointMapMeters {0.0};
        glm::dvec3 portalCenterMapMeters {0.0};
        double portalCrossTrackMeters = 0.0;
        double portalLateralSpeedMps = 0.0;
        double portalVelocityAngleRad = 0.0;
        double portalForwardAngleRad = 0.0;
        bool portalVelocityAligned = false;
        bool portalForwardAligned = false;
        bool portalCaptureReady = false;
        bool portalApproachHolding = false;

        Map::EntityId primaryConflictEntityId = 0;
        Map::EntityId nominalPrimaryConflictEntityId = 0;
        std::size_t dynamicCandidatesExamined = 0;
        std::size_t dynamicConflictsFound = 0;
        std::size_t nominalDynamicConflictsFound = 0;
        std::size_t avoidanceProbesExamined = 0;

        bool nominalStaticBlocked = false;
        std::size_t staticObstaclesExamined = 0;
        std::string nominalStaticObstacleId;
        std::uint32_t nominalStaticObstacleEntityId = 0;

        bool movingPrecisionAttempted = false;
        bool movingPassageFeasible = false;
        std::size_t movingGapCandidatesBuilt = 0;
        std::size_t movingGapPredictionsEvaluated = 0;
        std::size_t movingPassagesEvaluated = 0;

        // Last moving-passage evaluator result observed in the bounded
        // candidate loop. These are diagnostics only; steering authority still
        // depends solely on movingPassageFeasible + movingPassageStaticSafe.
        MovingPassage::Status movingPassageLastEvaluatorStatus =
            MovingPassage::Status::InvalidInput;
        double movingPassageRequiredPeakForwardAccelerationMps2 = 0.0;
        double movingPassageRequiredPeakReverseAccelerationMps2 = 0.0;
        double movingPassageRequiredPeakLateralAccelerationMps2 = 0.0;
        double movingPassageRequiredPeakVerticalAccelerationMps2 = 0.0;
        double movingPassageMinimumSampleClearanceMeters = 0.0;
        double movingPassageMinimumContinuousClearanceMeters = 0.0;

        Map::EntityId movingPrimaryObstacleEntityId = 0;
        Map::EntityId movingSecondaryObstacleEntityId = 0;
        glm::dvec3 movingPassageInitialAccelerationMapMps2 {0.0};
        glm::dvec3 movingPassageTargetMapMeters {0.0};
        bool movingPassageAuthorityUsed = false;

        // Exact-static proof of the same Hermite trajectory already accepted
        // by MovingPassageTrajectoryEvaluator. 12A-6b3a may consume this proof
        // only through the explicit allowSteeringAuthority gate above.
        bool movingPassageStaticProofAttempted = false;
        bool movingPassageStaticSafe = false;
        std::size_t movingPassageStaticIntervalsProven = 0;
        std::size_t movingPassageStaticObstaclesExamined = 0;
        double movingPassageStaticMaximumCurveDeviationMeters = 0.0;
        std::string movingPassageStaticBlockingObstacleId;
        std::uint32_t movingPassageStaticBlockingObstacleEntityId = 0;
    };

    [[nodiscard]] static Result plan(
        const AgentState& agent,
        const Goal& goal,
        const Map::QueryResult& dynamicCandidates,
        double dynamicResultAgeSeconds,
        const StaticQueries& staticQueries,
        const Policy& policy
    );

};

} // namespace game::navigation
