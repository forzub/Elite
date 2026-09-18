#include "NavigationRuntimePlanner.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace game::navigation
{
namespace
{

using Planner = NavigationRuntimePlanner;

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double lengthSquared = glm::dot(value, value);
    if (!finite(lengthSquared) || lengthSquared <= kEpsilon)
        return fallback;
    return value / std::sqrt(lengthSquared);
}

double angleBetweenUnitSafe(
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    const glm::dvec3 na = normalizedOr(a, glm::dvec3(0.0));
    const glm::dvec3 nb = normalizedOr(b, glm::dvec3(0.0));
    if (glm::dot(na, na) <= kEpsilon ||
        glm::dot(nb, nb) <= kEpsilon)
    {
        return 0.0;
    }

    return std::acos(
        std::clamp(glm::dot(na, nb), -1.0, 1.0)
    );
}

glm::dvec3 agentAngularVelocityMap(
    const Planner::AgentState& agent
) noexcept
{
    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    return
        right * agent.pitchRateRadPerSec +
        up * agent.yawRateRadPerSec +
        forward * agent.rollRateRadPerSec;
}

glm::dvec3 portalAlignmentAngularDemand(
    const Planner::AgentState& agent,
    const glm::dvec3& desiredForwardMap,
    const Planner::Goal& goal,
    const Planner::PortalTraversalPolicy& policy
) noexcept
{
    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 desired = normalizedOr(
        desiredForwardMap,
        forward
    );

    const glm::dvec3 cross = glm::cross(forward, desired);
    const double sinAngle = glm::length(cross);
    const double cosAngle = std::clamp(
        glm::dot(forward, desired),
        -1.0,
        1.0
    );
    const double angle = std::atan2(sinAngle, cosAngle);

    glm::dvec3 axis(0.0);
    if (sinAngle > kEpsilon)
        axis = cross / sinAngle;
    else if (cosAngle < 0.0)
        axis = normalizedOr(agent.upMap, glm::dvec3(0.0, 1.0, 0.0));

    glm::dvec3 demand =
        axis * (
            std::max(0.0, policy.orientationResponsePerSecond2) *
            angle
        ) -
        agentAngularVelocityMap(agent) *
            std::max(0.0, goal.angularDampingPerSecond);

    const double maxAngular =
        std::max(
            0.0,
            agent.angularCapability.maxAngularAccelerationRadPerSec2
        );
    const double magnitude = glm::length(demand);
    if (maxAngular > 0.0 &&
        magnitude > maxAngular &&
        magnitude > kEpsilon)
    {
        demand *= maxAngular / magnitude;
    }

    return demand;
}

world::navigation::NavigationMap::Vec3d toMapVec(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

world::navigation::NavigationSpace::Vec3d toSpaceVec(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

glm::dvec3 toGlm(
    const world::navigation::NavigationMap::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

glm::dvec3 toGlm(
    const world::navigation::NavigationSpace::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

NavigationRuntimePlanner::Bridge::Vec3d toBridgeVec(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

Planner::MovingPassage::Vec3d toTrajectoryVec(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

Planner::MovingPassage::Vec3d toTrajectoryVec(
    const Planner::Map::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

glm::dvec3 toGlm(
    const Planner::MovingPassage::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

const Planner::Map::Candidate* findCandidate(
    const Planner::Map::QueryResult& dynamicCandidates,
    Planner::Map::EntityId entityId
) noexcept
{
    for (const Planner::Map::Candidate& candidate : dynamicCandidates.candidates)
    {
        if (candidate.entityId == entityId)
            return &candidate;
    }
    return nullptr;
}

Planner::GapPredictor::BoundaryMotion boundaryMotion(
    const Planner::Map::Candidate& candidate,
    Planner::Map::Revision snapshotRevision
) noexcept
{
    Planner::GapPredictor::BoundaryMotion motion;
    motion.obstacleId = candidate.entityId;
    motion.snapshotRevision = snapshotRevision;
    motion.centerMapMeters = toTrajectoryVec(candidate.positionMapMeters);
    motion.linearVelocityMapMetersPerSec =
        toTrajectoryVec(candidate.velocityMapMetersPerSecond);
    motion.linearAccelerationMapMetersPerSec2 =
        toTrajectoryVec(candidate.accelerationMapMetersPerSecond2);
    motion.angularVelocityMapRadPerSec =
        toTrajectoryVec(candidate.angularVelocityMapRadPerSecond);
    motion.conservativeRadiusMeters = candidate.actorRadiusMeters;
    return motion;
}

bool movingPassageBodyBasis(
    const Planner::AgentState& agent,
    Planner::MovingPassage::Basis3d& out
) noexcept
{
    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0)
    );
    if (glm::dot(forward, forward) <= kEpsilon)
        return false;

    glm::dvec3 up = agent.upMap - forward * glm::dot(agent.upMap, forward);
    up = normalizedOr(up, glm::dvec3(0.0));
    if (glm::dot(up, up) <= kEpsilon)
        return false;

    const glm::dvec3 right = normalizedOr(
        glm::cross(up, forward),
        glm::dvec3(0.0)
    );
    if (glm::dot(right, right) <= kEpsilon)
        return false;

    out.right = toTrajectoryVec(right);
    out.up = toTrajectoryVec(up);
    out.forward = toTrajectoryVec(forward);
    return true;
}

Planner::MovingPassage::Vec3d precisionHullHalfExtents(
    const Planner::AgentState& agent
) noexcept
{
    const glm::dvec3& h = agent.hullHalfExtentsBodyMeters;
    if (finite(h) && h.x > 0.0 && h.y > 0.0 && h.z > 0.0)
        return toTrajectoryVec(h);

    const double fallback = std::max(0.0, agent.radiusMeters);
    return {fallback, fallback, fallback};
}

NavigationRuntimePlanner::Bridge::Intent holdIntent(
    const NavigationRuntimePlanner::AgentState& agent,
    const NavigationRuntimePlanner::Goal& goal,
    double urgency
) noexcept
{
    NavigationRuntimePlanner::Bridge::Intent intent;
    intent.revision = goal.revision;
    intent.emergency = goal.emergency || urgency >= 0.75;
    intent.hazardUrgency01 = std::clamp(
        std::max(goal.hazardUrgency01, urgency),
        0.0,
        1.0
    );

    const double response = std::max(0.0, goal.velocityResponsePerSecond);
    const glm::dvec3 linearDemand =
        -agent.velocityMapMetersPerSecond * response;

    const double angularDamping =
        std::max(0.0, goal.angularDampingPerSecond);
    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );
    const glm::dvec3 angularDemand =
        right * (-agent.pitchRateRadPerSec * angularDamping) +
        up * (-agent.yawRateRadPerSec * angularDamping) +
        forward * (-agent.rollRateRadPerSec * angularDamping);

    intent.idealLinearAccelerationDemandMapMps2 =
        toBridgeVec(linearDemand);
    intent.idealAngularAccelerationDemandMapRadPerSec2 =
        toBridgeVec(angularDemand);
    return intent;
}

bool validInput(
    const NavigationRuntimePlanner::AgentState& agent,
    const NavigationRuntimePlanner::Goal& goal,
    double dynamicResultAgeSeconds
) noexcept
{
    return
        finite(agent.positionMapMeters) &&
        finite(agent.velocityMapMetersPerSecond) &&
        finite(agent.accelerationMapMetersPerSecond2) &&
        finite(agent.radiusMeters) &&
        agent.radiusMeters >= 0.0 &&
        finite(agent.forwardMap) &&
        finite(agent.rightMap) &&
        finite(agent.upMap) &&
        finite(agent.pitchRateRadPerSec) &&
        finite(agent.yawRateRadPerSec) &&
        finite(agent.rollRateRadPerSec) &&
        finite(goal.targetPositionMapMeters) &&
        finite(goal.targetVelocityMapMetersPerSecond) &&
        finite(goal.targetAccelerationMapMetersPerSecond2) &&
        finite(goal.maximumTargetSpeedMps) &&
        goal.maximumTargetSpeedMps >= 0.0 &&
        finite(goal.velocityResponsePerSecond) &&
        goal.velocityResponsePerSecond >= 0.0 &&
        finite(goal.angularDampingPerSecond) &&
        goal.angularDampingPerSecond >= 0.0 &&
        finite(goal.arrivalRadiusMeters) &&
        goal.arrivalRadiusMeters >= 0.0 &&
        finite(goal.hazardUrgency01) &&
        finite(dynamicResultAgeSeconds) &&
        dynamicResultAgeSeconds >= 0.0;
}

struct StaticMovingTrajectoryProof
{
    bool attempted = false;
    bool safe = false;
    std::size_t intervalsProven = 0;
    std::size_t obstaclesExamined = 0;
    double maximumCurveDeviationMeters = 0.0;
    std::string blockingObstacleId;
    std::uint32_t blockingObstacleEntityId = 0;
};

StaticMovingTrajectoryProof proveMovingPassageAgainstStaticSpace(
    const Planner::MovingPassage::Result& passage,
    const Planner::Space& staticSpace,
    const Planner::Policy& policy
)
{
    StaticMovingTrajectoryProof proof;
    proof.attempted = true;

    const auto& trajectory = passage.trajectory;
    if (!passage.feasible || !trajectory.valid ||
        !finite(trajectory.conservativeHullRadiusMeters) ||
        trajectory.conservativeHullRadiusMeters < 0.0 ||
        !finite(policy.avoidance.staticAdditionalClearanceMeters) ||
        policy.avoidance.staticAdditionalClearanceMeters < 0.0)
    {
        return proof;
    }

    for (std::size_t i = 0;
         i < Planner::MovingPassage::kIntervals;
         ++i)
    {
        const double curveDeviation =
            trajectory.intervalCenterlineDeviationBoundsMeters[i];
        if (!finite(curveDeviation) || curveDeviation < 0.0)
            return proof;

        proof.maximumCurveDeviationMeters = std::max(
            proof.maximumCurveDeviationMeters,
            curveDeviation
        );

        const double continuousHullRadius =
            trajectory.conservativeHullRadiusMeters + curveDeviation;
        if (!finite(continuousHullRadius) || continuousHullRadius < 0.0)
            return proof;

        Planner::Space::SegmentQuery staticQuery;
        const auto& startCenter =
            trajectory.centerSamplesMapMeters[i];
        const auto& endCenter =
            trajectory.centerSamplesMapMeters[i + 1];
        staticQuery.startMapMeters = Planner::Space::Vec3d {
            startCenter.x, startCenter.y, startCenter.z
        };
        staticQuery.endMapMeters = Planner::Space::Vec3d {
            endCenter.x, endCenter.y, endCenter.z
        };
        staticQuery.envelope.radiusMeters = continuousHullRadius;
        staticQuery.envelope.additionalClearanceMeters =
            policy.avoidance.staticAdditionalClearanceMeters;
        staticQuery.requireSameRegion = true;

        const Planner::Space::SegmentQueryResult staticResult =
            staticSpace.querySegment(staticQuery);
        proof.obstaclesExamined += staticResult.obstaclesExamined;

        if (!staticResult.traversable)
        {
            proof.blockingObstacleId = staticResult.blockingObstacleId;
            proof.blockingObstacleEntityId =
                staticResult.blockingObstacleEntityId;
            return proof;
        }

        ++proof.intervalsProven;
    }

    proof.safe =
        proof.intervalsProven == Planner::MovingPassage::kIntervals;
    return proof;
}

void probeMovingPassage(
    const Planner::AgentState& agent,
    const Planner::Goal& goal,
    const Planner::Map::QueryResult& dynamicCandidates,
    const Planner::Space& staticSpace,
    const glm::dvec3& coarseTarget,
    const Planner::Policy& policy,
    const Planner::Avoidance::Result& local,
    Planner::Result& result
)
{
    const Planner::MovingPassagePolicy& precision = policy.movingPassage;
    if (!precision.enabled || local.nominalConflictsFound == 0)
        return;

    result.movingPrecisionAttempted = true;

    if (!finite(precision.durationSeconds) ||
        precision.durationSeconds <= 0.0 ||
        !finite(precision.hullAdditionalClearanceMeters) ||
        precision.hullAdditionalClearanceMeters < 0.0 ||
        !finite(precision.maximumAcceptedGapTravelAlignment) ||
        precision.maximumAcceptedGapTravelAlignment < 0.0 ||
        precision.maximumAcceptedGapTravelAlignment > 1.0 ||
        !finite(precision.maximumInitialAngularRateRadPerSec) ||
        precision.maximumInitialAngularRateRadPerSec < 0.0)
    {
        return;
    }

    const double maximumInitialAngularRate = std::max({
        std::abs(agent.pitchRateRadPerSec),
        std::abs(agent.yawRateRadPerSec),
        std::abs(agent.rollRateRadPerSec)
    });
    if (maximumInitialAngularRate >
        precision.maximumInitialAngularRateRadPerSec)
    {
        return;
    }

    const Planner::Map::EntityId primaryId =
        local.nominalPrimaryConflictEntityId != 0
            ? local.nominalPrimaryConflictEntityId
            : local.target.primaryConflictEntityId;
    if (primaryId == 0)
        return;

    const Planner::Map::Candidate* primary =
        findCandidate(dynamicCandidates, primaryId);
    if (primary == nullptr)
        return;

    const glm::dvec3 travelDelta =
        coarseTarget - agent.positionMapMeters;
    const glm::dvec3 travelDirection =
        normalizedOr(travelDelta, glm::dvec3(0.0));
    if (glm::dot(travelDirection, travelDirection) <= kEpsilon)
        return;

    Planner::GapBuilder::Query gapQuery;
    gapQuery.referencePointMapMeters =
        toTrajectoryVec(agent.positionMapMeters);
    gapQuery.travelDirectionMap = toTrajectoryVec(travelDirection);
    gapQuery.primary.obstacleId = primary->entityId;
    gapQuery.primary.snapshotRevision = dynamicCandidates.mapRevision;
    gapQuery.primary.centerMapMeters =
        toTrajectoryVec(primary->positionMapMeters);
    gapQuery.primary.conservativeRadiusMeters = primary->actorRadiusMeters;
    gapQuery.policy = precision.candidates;

    gapQuery.neighbors.reserve(dynamicCandidates.candidates.size());
    for (const Planner::Map::Candidate& candidate : dynamicCandidates.candidates)
    {
        if (candidate.entityId == agent.entityId ||
            candidate.entityId == primary->entityId)
        {
            continue;
        }

        Planner::GapBuilder::ObstacleWitness witness;
        witness.obstacleId = candidate.entityId;
        witness.snapshotRevision = dynamicCandidates.mapRevision;
        witness.centerMapMeters = toTrajectoryVec(candidate.positionMapMeters);
        witness.conservativeRadiusMeters = candidate.actorRadiusMeters;
        gapQuery.neighbors.push_back(witness);
    }

    const Planner::GapBuilder::Result gapCandidates =
        Planner::GapBuilder::build(gapQuery);
    result.movingGapCandidatesBuilt = gapCandidates.candidates.size();
    if (!gapCandidates.validInput || gapCandidates.candidates.empty())
        return;

    Planner::MovingPassage::Basis3d bodyBasis;
    if (!movingPassageBodyBasis(agent, bodyBasis))
        return;

    for (const Planner::GapBuilder::Candidate& gapCandidate :
         gapCandidates.candidates)
    {
        const Planner::Map::Candidate* secondary =
            findCandidate(
                dynamicCandidates,
                gapCandidate.neighborObstacleId
            );
        if (secondary == nullptr)
            continue;

        Planner::GapPredictor::Query predictionQuery;
        predictionQuery.travelDirectionMap =
            toTrajectoryVec(travelDirection);
        predictionQuery.primary =
            boundaryMotion(*primary, dynamicCandidates.mapRevision);
        predictionQuery.secondary =
            boundaryMotion(*secondary, dynamicCandidates.mapRevision);
        predictionQuery.horizonSeconds = precision.durationSeconds;
        predictionQuery.policy = precision.prediction;

        // Candidate discovery and prediction must describe the same aperture.
        predictionQuery.policy.boundaryClearanceMeters =
            precision.candidates.boundaryClearanceMeters;
        predictionQuery.policy.secondaryClearanceMeters =
            precision.candidates.secondaryClearanceMeters;
        predictionQuery.policy.maximumAbsSeparationTravelDot =
            std::min(
                predictionQuery.policy.maximumAbsSeparationTravelDot,
                precision.candidates.maximumAbsSeparationTravelDot
            );

        const Planner::GapPredictor::Result movingGap =
            Planner::GapPredictor::predict(predictionQuery);
        ++result.movingGapPredictionsEvaluated;

        if (movingGap.status !=
            Planner::GapPredictor::Status::OpenForHorizon)
        {
            continue;
        }

        Planner::MovingPassage::Query passageQuery;
        passageQuery.hull.halfExtentsBodyMeters =
            precisionHullHalfExtents(agent);
        passageQuery.hull.additionalClearanceMeters =
            precision.hullAdditionalClearanceMeters;
        passageQuery.movingGap = &movingGap;

        passageQuery.start.pose.centerMapMeters =
            toTrajectoryVec(agent.positionMapMeters);
        passageQuery.start.pose.bodyToMap = bodyBasis;
        passageQuery.start.linearVelocityMapMetersPerSec =
            toTrajectoryVec(agent.velocityMapMetersPerSecond);

        const auto& finalGap = movingGap.samples.back();
        passageQuery.end.pose.centerMapMeters =
            finalGap.gap.centerMapMeters;
        passageQuery.end.pose.bodyToMap = bodyBasis;

        const glm::dvec3 finalGapCenter =
            toGlm(finalGap.gap.centerMapMeters);
        const double distanceToGap =
            glm::length(finalGapCenter - agent.positionMapMeters);
        const double crossingSpeed = std::min(
            goal.maximumTargetSpeedMps,
            distanceToGap / precision.durationSeconds
        );
        const glm::dvec3 finalGapVelocity =
            toGlm(finalGap.gapCenterVelocityMapMetersPerSec) +
            travelDirection * std::max(0.0, crossingSpeed);
        passageQuery.end.linearVelocityMapMetersPerSec =
            toTrajectoryVec(finalGapVelocity);

        passageQuery.durationSeconds = precision.durationSeconds;
        passageQuery.linearCapability = agent.linearCapability;
        passageQuery.angularCapability = agent.angularCapability;
        passageQuery.controlMode = agent.controlMode;
        passageQuery.assistedMaxVelocityToForwardAngleRad =
            agent.assistedMaxVelocityToForwardAngleRad;
        passageQuery.maximumAcceptedGapTravelAlignment =
            precision.maximumAcceptedGapTravelAlignment;

        const Planner::MovingPassage::Result passage =
            Planner::MovingPassage::evaluate(passageQuery);
        ++result.movingPassagesEvaluated;

        result.movingPassageLastEvaluatorStatus = passage.status;
        result.movingPassageRequiredPeakForwardAccelerationMps2 =
            passage.requiredPeakForwardAccelerationMetersPerSec2;
        result.movingPassageRequiredPeakReverseAccelerationMps2 =
            passage.requiredPeakReverseAccelerationMetersPerSec2;
        result.movingPassageRequiredPeakLateralAccelerationMps2 =
            passage.requiredPeakLateralAccelerationMetersPerSec2;
        result.movingPassageRequiredPeakVerticalAccelerationMps2 =
            passage.requiredPeakVerticalAccelerationMetersPerSec2;
        result.movingPassageMinimumSampleClearanceMeters =
            passage.minimumSampleClearanceMeters;
        result.movingPassageMinimumContinuousClearanceMeters =
            passage.minimumContinuousClearanceBoundMeters;

        if (!passage.feasible)
            continue;

        result.movingPassageFeasible = true;
        result.movingPrimaryObstacleEntityId = primary->entityId;
        result.movingSecondaryObstacleEntityId = secondary->entityId;
        result.movingPassageInitialAccelerationMapMps2 =
            toGlm(passage.initialLinearAccelerationMapMetersPerSec2);
        result.movingPassageTargetMapMeters =
            toGlm(passage.trajectory.centerSamplesMapMeters.back());

        const StaticMovingTrajectoryProof staticProof =
            proveMovingPassageAgainstStaticSpace(
                passage,
                staticSpace,
                policy
            );
        result.movingPassageStaticProofAttempted = staticProof.attempted;
        result.movingPassageStaticSafe = staticProof.safe;
        result.movingPassageStaticIntervalsProven =
            staticProof.intervalsProven;
        result.movingPassageStaticObstaclesExamined =
            staticProof.obstaclesExamined;
        result.movingPassageStaticMaximumCurveDeviationMeters =
            staticProof.maximumCurveDeviationMeters;
        result.movingPassageStaticBlockingObstacleId =
            staticProof.blockingObstacleId;
        result.movingPassageStaticBlockingObstacleEntityId =
            staticProof.blockingObstacleEntityId;

        if (staticProof.safe)
            return;

        // Another already-bounded gap candidate may still be dynamically and
        // statically valid. Keep the accepted <=8 candidate bound and continue
        // rather than turning one static blocker into a global hold.
    }
}

} // namespace

NavigationRuntimePlanner::Bridge::Intent
NavigationRuntimePlanner::mapIntentToWorld(
    const Bridge::Intent& mapIntent,
    const Map::WorkingFrame& workingFrame
)
{
    const auto toGlmAxis =
        [](const Map::Vec3d& axis)
        {
            return glm::dvec3(axis.x, axis.y, axis.z);
        };

    const glm::dvec3 xAxis =
        normalizedOr(toGlmAxis(workingFrame.xAxisSystem), glm::dvec3(0.0));
    const glm::dvec3 yAxis =
        normalizedOr(toGlmAxis(workingFrame.yAxisSystem), glm::dvec3(0.0));
    const glm::dvec3 zAxis =
        normalizedOr(toGlmAxis(workingFrame.zAxisSystem), glm::dvec3(0.0));

    if (glm::dot(xAxis, xAxis) <= kEpsilon ||
        glm::dot(yAxis, yAxis) <= kEpsilon ||
        glm::dot(zAxis, zAxis) <= kEpsilon ||
        std::abs(glm::dot(xAxis, yAxis)) > 1.0e-6 ||
        std::abs(glm::dot(xAxis, zAxis)) > 1.0e-6 ||
        std::abs(glm::dot(yAxis, zAxis)) > 1.0e-6)
    {
        throw std::invalid_argument(
            "NavigationRuntimePlanner working frame is invalid"
        );
    }

    const auto mapVectorToWorld =
        [&](const Bridge::Vec3d& value)
        {
            const glm::dvec3 world =
                xAxis * value.x +
                yAxis * value.y +
                zAxis * value.z;
            return Bridge::Vec3d {world.x, world.y, world.z};
        };

    Bridge::Intent worldIntent = mapIntent;
    worldIntent.idealLinearAccelerationDemandMapMps2 =
        mapVectorToWorld(
            mapIntent.idealLinearAccelerationDemandMapMps2
        );
    worldIntent.idealAngularAccelerationDemandMapRadPerSec2 =
        mapVectorToWorld(
            mapIntent.idealAngularAccelerationDemandMapRadPerSec2
        );
    return worldIntent;
}

NavigationRuntimePlanner::Result NavigationRuntimePlanner::plan(
    const AgentState& agent,
    const Goal& goal,
    const Map::QueryResult& dynamicCandidates,
    double dynamicResultAgeSeconds,
    const Space& staticSpace,
    const Policy& policy
)
{
    Result result;
    result.intent.revision = goal.revision;
    result.mapRevision = dynamicCandidates.mapRevision;
    result.mapSourceRevision = dynamicCandidates.sourceRevision;

    if (!validInput(agent, goal, dynamicResultAgeSeconds))
        return result;

    Space::CorridorQuery corridorQuery;
    corridorQuery.startMapMeters = toSpaceVec(agent.positionMapMeters);
    corridorQuery.endMapMeters = toSpaceVec(goal.targetPositionMapMeters);
    corridorQuery.envelope.radiusMeters = agent.radiusMeters;
    corridorQuery.envelope.additionalClearanceMeters =
        policy.avoidance.staticAdditionalClearanceMeters;

    const Space::CostedCorridorResult corridor =
        staticSpace.queryCostedCorridor(corridorQuery, policy.corridor);

    result.spaceRevision = corridor.spaceRevision;
    result.spaceSourceRevision = corridor.sourceRevision;
    result.staticRegionPath = corridor.regionPath;
    result.staticPortalPath = corridor.portalPath;
    result.staticPortalCentersMapMeters = corridor.portalCentersMapMeters;

    if (!corridor.found)
    {
        result.status = Status::StaticHold;
        result.selectedTargetMapMeters = agent.positionMapMeters;
        result.coarseWaypointMapMeters = agent.positionMapMeters;
        result.intent = holdIntent(agent, goal, 0.5);
        return result;
    }

    glm::dvec3 coarseTarget = goal.targetPositionMapMeters;
    glm::dvec3 coarseVelocity = goal.targetVelocityMapMetersPerSecond;
    glm::dvec3 coarseAcceleration = goal.targetAccelerationMapMetersPerSecond2;

    if (!corridor.portalCentersMapMeters.empty())
    {
        coarseTarget = toGlm(corridor.portalCentersMapMeters.front());
        coarseVelocity = glm::dvec3(0.0);
        coarseAcceleration = glm::dvec3(0.0);
        result.usedPortalWaypoint = true;
    }
    result.coarseWaypointMapMeters = coarseTarget;

    Avoidance::Query localQuery;
    localQuery.horizon.agent.entityId = agent.entityId;
    localQuery.horizon.agent.positionMapMeters = toMapVec(agent.positionMapMeters);
    localQuery.horizon.agent.velocityMapMetersPerSecond =
        toMapVec(agent.velocityMapMetersPerSecond);
    localQuery.horizon.agent.accelerationMapMetersPerSecond2 =
        toMapVec(agent.accelerationMapMetersPerSecond2);
    localQuery.horizon.agent.radiusMeters = agent.radiusMeters;

    localQuery.horizon.nominalTarget.positionMapMeters = toMapVec(coarseTarget);
    localQuery.horizon.nominalTarget.velocityMapMetersPerSecond =
        toMapVec(coarseVelocity);
    localQuery.horizon.nominalTarget.accelerationMapMetersPerSecond2 =
        toMapVec(coarseAcceleration);
    localQuery.horizon.dynamicResultAgeSeconds = dynamicResultAgeSeconds;
    localQuery.horizon.policy = policy.horizon;
    localQuery.avoidance = policy.avoidance;
    localQuery.avoidance.nominalTargetIsProvenPortalBoundary =
        result.usedPortalWaypoint;

    const Avoidance::Result local = Avoidance{}.evaluate(
        localQuery,
        dynamicCandidates,
        staticSpace
    );

    result.adjustedTarget = local.adjustedTarget;
    result.avoidanceProbesExamined = local.targetProbesExamined;
    result.primaryConflictEntityId = local.target.primaryConflictEntityId;
    result.nominalPrimaryConflictEntityId =
        local.nominalPrimaryConflictEntityId;
    result.dynamicCandidatesExamined = local.target.candidatesExamined;
    result.dynamicConflictsFound = local.target.conflictsFound;
    result.nominalDynamicConflictsFound =
        local.nominalConflictsFound;
    result.nominalStaticBlocked =
        local.nominalStaticBlocked;
    result.staticObstaclesExamined =
        local.staticObstaclesExamined;
    result.nominalStaticObstacleId =
        local.nominalStaticObstacleId;
    result.nominalStaticObstacleEntityId =
        local.nominalStaticObstacleEntityId;
    result.selectedTargetMapMeters = toGlm(local.target.targetPositionMapMeters);

    // The bounded moving-gap / moving-passage chain also proves the exact
    // accepted Hermite trajectory against NavigationSpace exact-static
    // geometry. Merely running this precision path is still observe-only;
    // steering changes only through the explicit 12A-6b3a authority gate below.
    probeMovingPassage(
        agent,
        goal,
        dynamicCandidates,
        staticSpace,
        coarseTarget,
        policy,
        local,
        result
    );

    // Stage 12A-6b3a: steering authority is opt-in and may only use the exact
    // first control sample of the same Hermite trajectory that passed both the
    // moving-aperture proof and the continuous exact-static proof. A stale
    // dynamic result can never gain this authority.
    if (policy.movingPassage.allowSteeringAuthority &&
        local.status != Avoidance::Status::StaleHold &&
        result.movingPassageFeasible &&
        result.movingPassageStaticSafe)
    {
        result.status = Status::MovingPassageClear;
        result.movingPassageAuthorityUsed = true;
        result.safeProgressTargetDemonstrated = true;
        result.adjustedTarget = false;
        result.selectedTargetMapMeters =
            result.movingPassageTargetMapMeters;

        // Reuse the existing stabilized angular-control semantics, but replace
        // only linear demand with the already-proven Hermite control sample.
        // No second desired-velocity or trajectory solve occurs here.
        result.intent = holdIntent(agent, goal, 0.0);
        result.intent.idealLinearAccelerationDemandMapMps2 =
            toBridgeVec(result.movingPassageInitialAccelerationMapMps2);
        return result;
    }

    switch (local.status)
    {
        case Avoidance::Status::NominalClear:
            result.status = Status::NominalClear;
            break;
        case Avoidance::Status::AdjustedClear:
            result.status = Status::AdjustedClear;
            break;
        case Avoidance::Status::ConflictHold:
            result.status = Status::ConflictHold;
            result.intent = holdIntent(agent, goal, 1.0);
            return result;
        case Avoidance::Status::StaleHold:
            result.status = Status::StaleHold;
            result.intent = holdIntent(agent, goal, 0.75);
            return result;
        case Avoidance::Status::StaticHold:
        default:
            result.status = Status::StaticHold;
            result.intent = holdIntent(agent, goal, 0.5);
            return result;
    }

    result.safeProgressTargetDemonstrated =
        local.target.safeProgressTargetDemonstrated;

    const glm::dvec3 selectedTarget =
        toGlm(local.target.targetPositionMapMeters);
    const glm::dvec3 delta = selectedTarget - agent.positionMapMeters;
    const double distance = glm::length(delta);

    glm::dvec3 desiredVelocity(0.0);
    const bool trueTerminal =
        !result.usedPortalWaypoint &&
        local.target.targetMode == Horizon::TargetMode::Terminal;

    if (trueTerminal && distance <= goal.arrivalRadiusMeters)
    {
        desiredVelocity = goal.targetVelocityMapMetersPerSecond;
    }
    else if (distance > kEpsilon)
    {
        double speed = goal.maximumTargetSpeedMps;

        if (trueTerminal)
        {
            const double brakingDistance = std::max(
                0.0,
                distance - goal.arrivalRadiusMeters
            );
            const double brakingAcceleration =
                std::max(kEpsilon, policy.horizon.maxBrakingAccelerationMetersPerSecond2);
            const double brakingLimitedSpeed =
                std::sqrt(2.0 * brakingAcceleration * brakingDistance);
            speed = std::min(speed, brakingLimitedSpeed);
        }

        desiredVelocity = glm::normalize(delta) * speed;
        if (trueTerminal)
            desiredVelocity += goal.targetVelocityMapMetersPerSecond;
    }

    result.desiredVelocityMapMetersPerSecond = desiredVelocity;

    result.intent.revision = goal.revision;
    result.intent.emergency = goal.emergency;
    result.intent.hazardUrgency01 =
        std::clamp(goal.hazardUrgency01, 0.0, 1.0);

    const glm::dvec3 linearDemand =
        (desiredVelocity - agent.velocityMapMetersPerSecond) *
        goal.velocityResponsePerSecond;

    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );
    const glm::dvec3 angularDemand =
        right * (-agent.pitchRateRadPerSec * goal.angularDampingPerSecond) +
        up * (-agent.yawRateRadPerSec * goal.angularDampingPerSecond) +
        forward * (-agent.rollRateRadPerSec * goal.angularDampingPerSecond);

    result.intent.idealLinearAccelerationDemandMapMps2 =
        toBridgeVec(linearDemand);
    result.intent.idealAngularAccelerationDemandMapRadPerSec2 =
        toBridgeVec(angularDemand);
    return result;
}

} // namespace game::navigation
