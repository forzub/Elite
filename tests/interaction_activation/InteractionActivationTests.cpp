#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "src/game/simulation/activation/ActivationPlanner.h"
#include "src/game/simulation/activation/ActivationExecutionPolicy.h"
#include "src/game/simulation/activation/ActivationSpatialIndex.h"
#include "src/game/simulation/activation/InteractionHorizon.h"
#include "src/game/simulation/activation/SpatialBounds.h"
#include "src/game/diagnostics/ActivationCadenceLab.h"

namespace
{
class TestFailure final : public std::runtime_error
{
public:
    explicit TestFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

[[noreturn]] void fail(const char* expression, const char* file, int line)
{
    throw TestFailure(
        std::string(file) + ':' + std::to_string(line) +
        ": check failed: " + expression
    );
}

#define REQUIRE(expression) \
    do \
    { \
        if (!(expression)) \
            fail(#expression, __FILE__, __LINE__); \
    } while (false)

bool approx(double a, double b, double eps = 1e-6)
{
    return std::abs(a - b) <= eps;
}

using game::simulation::activation::InteractionHorizonPolicy;
using game::simulation::activation::KinematicPoint;
using game::simulation::activation::SpatialBounds;
using game::simulation::activation::ActivationAnchor;
using game::simulation::activation::ActivationAnchorKind;
using game::simulation::SimulationMode;

void testLogicalSizeProducesDifferentBroadBounds()
{
    LogicalDimensions cobra;
    cobra.enabled = true;
    cobra.length = 22.2f;
    cobra.width = 26.0f;
    cobra.height = 5.0f;

    LogicalDimensions station;
    station.enabled = true;
    station.length = 5021.38f;
    station.width = 4000.0f;
    station.height = 4089.56f;

    const auto cobraBounds =
        game::simulation::activation::makeSpatialBounds(cobra);
    const auto stationBounds =
        game::simulation::activation::makeSpatialBounds(station);

    REQUIRE(cobraBounds.interactionRadiusMeters > 10.0);
    REQUIRE(cobraBounds.interactionRadiusMeters < 25.0);
    REQUIRE(stationBounds.interactionRadiusMeters > 3000.0);
    REQUIRE(
        stationBounds.interactionRadiusMeters >
        cobraBounds.interactionRadiusMeters * 150.0
    );
}

void testLargeStationActivatesEarlierThanSmallShip()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;
    policy.safetyMarginMeters = 25.0;

    KinematicPoint observer;
    observer.bounds = SpatialBounds{18.0};

    KinematicPoint smallShip;
    smallShip.positionMeters = {3000.0, 0.0, 0.0};
    smallShip.bounds = SpatialBounds{18.0};

    KinematicPoint station = smallShip;
    station.bounds = SpatialBounds{3800.0};

    const auto shipPrediction =
        game::simulation::activation::evaluateInteractionHorizon(
            observer,
            smallShip,
            policy
        );

    const auto stationPrediction =
        game::simulation::activation::evaluateInteractionHorizon(
            observer,
            station,
            policy
        );

    REQUIRE(!shipPrediction.currentlyWithinEnvelope);
    REQUIRE(stationPrediction.currentlyWithinEnvelope);
}

void testFastClosingPairPrewarmsBeforeDistanceThreshold()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;
    policy.safetyMarginMeters = 25.0;

    KinematicPoint a;
    a.positionMeters = {-10000.0, 0.0, 0.0};
    a.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    a.bounds = SpatialBounds{18.0};

    KinematicPoint b;
    b.positionMeters = {10000.0, 0.0, 0.0};
    b.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{18.0};

    const auto prediction =
        game::simulation::activation::evaluateInteractionHorizon(a, b, policy);

    REQUIRE(!prediction.currentlyWithinEnvelope);
    REQUIRE(prediction.closingAtSampleTime);
    REQUIRE(prediction.entersEnvelopeWithinHorizon);
    REQUIRE(prediction.timeToClosestSeconds > 3.0);
    REQUIRE(prediction.timeToClosestSeconds < 3.5);
    REQUIRE(approx(prediction.closestCenterDistanceMeters, 0.0));
}

void testDivergingPairDoesNotPrewarm()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;

    KinematicPoint a;
    a.positionMeters = {-10000.0, 0.0, 0.0};
    a.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    a.bounds = SpatialBounds{18.0};

    KinematicPoint b;
    b.positionMeters = {10000.0, 0.0, 0.0};
    b.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{18.0};

    const auto prediction =
        game::simulation::activation::evaluateInteractionHorizon(a, b, policy);

    REQUIRE(!prediction.closingAtSampleTime);
    REQUIRE(!prediction.entersEnvelopeWithinHorizon);
    REQUIRE(approx(prediction.timeToClosestSeconds, 0.0));
}

void testShortHorizonDoesNotWakeFutureCollisionTooEarly()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 2.0;
    policy.safetyMarginMeters = 25.0;

    KinematicPoint a;
    a.positionMeters = {-10000.0, 0.0, 0.0};
    a.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    a.bounds = SpatialBounds{18.0};

    KinematicPoint b;
    b.positionMeters = {10000.0, 0.0, 0.0};
    b.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{18.0};

    const auto prediction =
        game::simulation::activation::evaluateInteractionHorizon(a, b, policy);

    REQUIRE(prediction.closingAtSampleTime);
    REQUIRE(!prediction.entersEnvelopeWithinHorizon);
    REQUIRE(approx(prediction.timeToClosestSeconds, 2.0));
}

void testGameplayRangeIsIndependentFromPhysicalRadius()
{
    InteractionHorizonPolicy physicalOnly;
    physicalOnly.lookAheadSeconds = 5.0;
    physicalOnly.safetyMarginMeters = 0.0;

    InteractionHorizonPolicy weaponCapable = physicalOnly;
    weaponCapable.gameplayRangeMeters = 5000.0;

    KinematicPoint a;
    a.bounds = SpatialBounds{20.0};

    KinematicPoint b;
    b.positionMeters = {4000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{20.0};

    const auto noWeapon =
        game::simulation::activation::evaluateInteractionHorizon(
            a,
            b,
            physicalOnly
        );

    const auto weapon =
        game::simulation::activation::evaluateInteractionHorizon(
            a,
            b,
            weaponCapable
        );

    REQUIRE(!noWeapon.currentlyWithinEnvelope);
    REQUIRE(weapon.currentlyWithinEnvelope);
}

void testPlayerIsPinnedActiveInShadowMode()
{
    KinematicPoint subject;
    subject.bounds = SpatialBounds{18.0};

    std::vector<ActivationAnchor> anchors;

    const auto decision =
        game::simulation::activation::evaluateActivationShadow(
            EntityId{1},
            0,
            subject,
            SimulationMode::Active,
            true,
            anchors,
            InteractionHorizonPolicy{}
        );

    REQUIRE(decision.desiredMode == SimulationMode::Active);
    REQUIRE(
        decision.reason ==
        game::simulation::activation::ActivationReason::PlayerPinnedActive
    );
    REQUIRE(!decision.hasAnchor);
}

void testCurrentLargeStaticAnchorKeepsShipActive()
{
    KinematicPoint subject;
    subject.bounds = SpatialBounds{18.0};

    ActivationAnchor station;
    station.id = EntityId{77};
    station.kind = ActivationAnchorKind::StaticObject;
    station.systemId = 0;
    station.kinematics.positionMeters = {3000.0, 0.0, 0.0};
    station.kinematics.bounds = SpatialBounds{3800.0};

    const auto decision =
        game::simulation::activation::evaluateActivationShadow(
            EntityId{2},
            0,
            subject,
            SimulationMode::Active,
            false,
            {station},
            InteractionHorizonPolicy{}
        );

    REQUIRE(decision.desiredMode == SimulationMode::Active);
    REQUIRE(decision.hasAnchor);
    REQUIRE(decision.anchorId == EntityId{77});
    REQUIRE(decision.anchorKind == ActivationAnchorKind::StaticObject);
}

void testFutureShipCollisionRequestsPrewarm()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;

    KinematicPoint subject;
    subject.positionMeters = {-10000.0, 0.0, 0.0};
    subject.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    subject.bounds = SpatialBounds{18.0};

    ActivationAnchor other;
    other.id = EntityId{3};
    other.kind = ActivationAnchorKind::Ship;
    other.systemId = 0;
    other.kinematics.positionMeters = {10000.0, 0.0, 0.0};
    other.kinematics.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    other.kinematics.bounds = SpatialBounds{18.0};

    const auto decision =
        game::simulation::activation::evaluateActivationShadow(
            EntityId{2},
            0,
            subject,
            SimulationMode::Active,
            false,
            {other},
            policy
        );

    REQUIRE(decision.desiredMode == SimulationMode::Prewarm);
    REQUIRE(
        decision.reason ==
        game::simulation::activation::ActivationReason::PredictedInteraction
    );
    REQUIRE(decision.hasAnchor);
    REQUIRE(decision.prediction.timeToClosestSeconds > 3.0);
    REQUIRE(decision.prediction.timeToClosestSeconds < 3.5);
}

void testNoInteractionRequestsCoarse()
{
    KinematicPoint subject;
    subject.positionMeters = {-10000.0, 0.0, 0.0};
    subject.velocityMetersPerSecond = {-100.0, 0.0, 0.0};
    subject.bounds = SpatialBounds{18.0};

    ActivationAnchor other;
    other.id = EntityId{3};
    other.kind = ActivationAnchorKind::Ship;
    other.systemId = 0;
    other.kinematics.positionMeters = {10000.0, 0.0, 0.0};
    other.kinematics.velocityMetersPerSecond = {100.0, 0.0, 0.0};
    other.kinematics.bounds = SpatialBounds{18.0};

    const auto decision =
        game::simulation::activation::evaluateActivationShadow(
            EntityId{2},
            0,
            subject,
            SimulationMode::Active,
            false,
            {other},
            InteractionHorizonPolicy{}
        );

    REQUIRE(decision.desiredMode == SimulationMode::Coarse);
    REQUIRE(
        decision.reason ==
        game::simulation::activation::ActivationReason::NoInteractionWithinHorizon
    );
}

void testOtherSystemsDoNotAffectShadowDecision()
{
    KinematicPoint subject;
    subject.bounds = SpatialBounds{18.0};

    ActivationAnchor impossibleNeighbor;
    impossibleNeighbor.id = EntityId{3};
    impossibleNeighbor.kind = ActivationAnchorKind::Ship;
    impossibleNeighbor.systemId = 1;
    impossibleNeighbor.kinematics.bounds = SpatialBounds{100000.0};

    const auto decision =
        game::simulation::activation::evaluateActivationShadow(
            EntityId{2},
            0,
            subject,
            SimulationMode::Active,
            false,
            {impossibleNeighbor},
            InteractionHorizonPolicy{}
        );

    REQUIRE(decision.desiredMode == SimulationMode::Coarse);
    REQUIRE(
        decision.reason ==
        game::simulation::activation::ActivationReason::NoComparableAnchors
    );
}


void testActiveDemotionUsesHysteresisLadder()
{
    using namespace game::simulation::activation;

    ActivationHysteresisPolicy policy;
    policy.activeReleaseDelaySeconds = 2.0;
    policy.prewarmReleaseDelaySeconds = 1.0;

    ActivationPlanState state;
    state.plannedMode = SimulationMode::Active;
    state.modeEnteredServerTimeSeconds = 0.0;
    state.releaseNotBeforeServerTimeSeconds = 2.0;

    const auto stillActive =
        updateActivationPlan(state, SimulationMode::Coarse, 1.0, policy);
    REQUIRE(stillActive.plannedMode == SimulationMode::Active);
    REQUIRE(stillActive.transition == ActivationPlanTransition::None);

    const auto toPrewarm =
        updateActivationPlan(state, SimulationMode::Coarse, 2.1, policy);
    REQUIRE(toPrewarm.plannedMode == SimulationMode::Prewarm);
    REQUIRE(
        toPrewarm.transition ==
        ActivationPlanTransition::DemoteToPrewarm
    );

    const auto holdPrewarm =
        updateActivationPlan(state, SimulationMode::Coarse, 2.8, policy);
    REQUIRE(holdPrewarm.plannedMode == SimulationMode::Prewarm);

    const auto toCoarse =
        updateActivationPlan(state, SimulationMode::Coarse, 3.2, policy);
    REQUIRE(toCoarse.plannedMode == SimulationMode::Coarse);
    REQUIRE(
        toCoarse.transition ==
        ActivationPlanTransition::DemoteToCoarse
    );
}

void testPromotionIsImmediate()
{
    using namespace game::simulation::activation;

    ActivationPlanState state;
    state.plannedMode = SimulationMode::Coarse;

    const auto prewarm =
        updateActivationPlan(
            state,
            SimulationMode::Prewarm,
            10.0,
            ActivationHysteresisPolicy{}
        );

    REQUIRE(prewarm.plannedMode == SimulationMode::Prewarm);
    REQUIRE(
        prewarm.transition ==
        ActivationPlanTransition::PromoteToPrewarm
    );

    const auto active =
        updateActivationPlan(
            state,
            SimulationMode::Active,
            10.2,
            ActivationHysteresisPolicy{}
        );

    REQUIRE(active.plannedMode == SimulationMode::Active);
    REQUIRE(
        active.transition ==
        ActivationPlanTransition::PromoteToActive
    );
}

void testGameplayClaimCanRaisePhysicalCoarseToActive()
{
    using namespace game::simulation::activation;

    ActivationShadowDecision physical;
    physical.subjectId = EntityId{42};
    physical.currentMode = SimulationMode::Active;
    physical.desiredMode = SimulationMode::Coarse;
    physical.reason = ActivationReason::NoInteractionWithinHorizon;

    ActivationClaim combat;
    combat.subjectId = EntityId{42};
    combat.sourceId = EntityId{77};
    combat.systemId = 0;
    combat.minimumMode = SimulationMode::Active;
    combat.kind = ActivationClaimKind::Combat;
    combat.expiresAtServerTimeSeconds = 20.0;

    ActivationPlanState state;
    state.plannedMode = SimulationMode::Coarse;

    const auto decision = evaluateActivationPlan(
        state,
        physical,
        0,
        {combat},
        12.0,
        ActivationHysteresisPolicy{}
    );

    REQUIRE(decision.claimDecision.hasClaim);
    REQUIRE(
        decision.claimDecision.kind == ActivationClaimKind::Combat
    );
    REQUIRE(
        decision.claimDecision.requestedMode == SimulationMode::Active
    );
    REQUIRE(decision.planUpdate.plannedMode == SimulationMode::Active);
}

void testExpiredGameplayClaimDoesNotKeepEntityActive()
{
    using namespace game::simulation::activation;

    ActivationShadowDecision physical;
    physical.subjectId = EntityId{42};
    physical.currentMode = SimulationMode::Active;
    physical.desiredMode = SimulationMode::Coarse;
    physical.reason = ActivationReason::NoInteractionWithinHorizon;

    ActivationClaim expired;
    expired.subjectId = EntityId{42};
    expired.sourceId = EntityId{77};
    expired.systemId = 0;
    expired.minimumMode = SimulationMode::Active;
    expired.kind = ActivationClaimKind::ProjectileThreat;
    expired.expiresAtServerTimeSeconds = 5.0;

    ActivationPlanState state;
    state.plannedMode = SimulationMode::Coarse;

    const auto decision = evaluateActivationPlan(
        state,
        physical,
        0,
        {expired},
        6.0,
        ActivationHysteresisPolicy{}
    );

    REQUIRE(!decision.claimDecision.hasClaim);
    REQUIRE(
        decision.claimDecision.requestedMode == SimulationMode::Coarse
    );
    REQUIRE(decision.planUpdate.plannedMode == SimulationMode::Coarse);
}

void testClaimCannotLowerPhysicalDemand()
{
    using namespace game::simulation::activation;

    ActivationClaim invalidLowDemand;
    invalidLowDemand.subjectId = EntityId{42};
    invalidLowDemand.sourceId = EntityId{77};
    invalidLowDemand.systemId = 0;
    invalidLowDemand.minimumMode = SimulationMode::Coarse;
    invalidLowDemand.kind = ActivationClaimKind::Docking;

    const auto evaluation = evaluateActivationClaims(
        EntityId{42},
        0,
        SimulationMode::Active,
        {invalidLowDemand},
        1.0
    );

    REQUIRE(!evaluation.hasClaim);
    REQUIRE(evaluation.requestedMode == SimulationMode::Active);
}


void testSpatialBroadphasePrunesDistantAnchorsWithoutChangingDecision()
{
    using namespace game::simulation::activation;

    std::vector<ActivationAnchor> anchors;

    ActivationAnchor self;
    self.id = EntityId{1};
    self.kind = ActivationAnchorKind::Ship;
    self.systemId = 0;
    self.kinematics.positionMeters = {0.0, 0.0, 0.0};
    self.kinematics.bounds = SpatialBounds{18.0};
    anchors.push_back(self);

    ActivationAnchor near;
    near.id = EntityId{2};
    near.kind = ActivationAnchorKind::Ship;
    near.systemId = 0;
    near.kinematics.positionMeters = {40.0, 0.0, 0.0};
    near.kinematics.bounds = SpatialBounds{18.0};
    anchors.push_back(near);

    for (std::uint32_t i = 0; i < 100; ++i)
    {
        ActivationAnchor far;
        far.id = EntityId{100 + i};
        far.kind = ActivationAnchorKind::Ship;
        far.systemId = 0;
        far.kinematics.positionMeters = {
            100000.0 + static_cast<double>(i) * 20000.0,
            0.0,
            0.0
        };
        far.kinematics.bounds = SpatialBounds{18.0};
        anchors.push_back(far);
    }

    KinematicPoint subject = self.kinematics;
    InteractionHorizonPolicy policy;

    const auto allPairs = evaluateActivationShadow(
        self.id,
        0,
        subject,
        SimulationMode::Active,
        false,
        anchors,
        policy
    );

    ActivationSpatialIndex index;
    index.rebuild(anchors);
    const auto query = index.query(0, subject, policy);

    const auto indexed = evaluateActivationShadowCandidates(
        self.id,
        0,
        subject,
        SimulationMode::Active,
        false,
        anchors,
        query.candidateIndices,
        policy,
        query.usedFallback
    );

    REQUIRE(!query.usedFallback);
    REQUIRE(query.candidateIndices.size() < anchors.size() / 4);
    REQUIRE(indexed.desiredMode == allPairs.desiredMode);
    REQUIRE(indexed.reason == allPairs.reason);
    REQUIRE(indexed.anchorId == allPairs.anchorId);
    REQUIRE(indexed.desiredMode == SimulationMode::Active);
}


void testSpatialBroadphaseRemovesSharedOrbitalBulkVelocity()
{
    using namespace game::simulation::activation;

    KinematicPoint subject;
    subject.positionMeters = {0.0, 0.0, 0.0};
    subject.velocityMetersPerSecond = {30000.0, 0.0, 0.0};
    subject.bounds = SpatialBounds{18.0};

    ActivationAnchor near;
    near.id = EntityId{2};
    near.kind = ActivationAnchorKind::Ship;
    near.systemId = 0;
    near.kinematics.positionMeters = {1000.0, 0.0, 0.0};
    near.kinematics.velocityMetersPerSecond = {30010.0, 0.0, 0.0};
    near.kinematics.bounds = SpatialBounds{18.0};

    ActivationAnchor far = near;
    far.id = EntityId{3};
    far.kinematics.positionMeters = {250000.0, 0.0, 0.0};
    far.kinematics.velocityMetersPerSecond = {29990.0, 0.0, 0.0};

    std::vector<ActivationAnchor> anchors{near, far};

    ActivationSpatialIndexConfig config;
    config.cellSizeMeters = 10000.0;
    config.maxVisitedCellsPerQuery = 4096;

    ActivationSpatialIndex index(config);
    index.rebuild(anchors);

    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;

    const auto query = index.query(0, subject, policy);

    // The previous absolute-speed bound saw roughly 30 km/s + 30 km/s and
    // expanded this query hundreds of kilometres, forcing fallback. Common
    // orbital drift must cancel from a relative interaction broad-phase.
    REQUIRE(!query.usedFallback);
    REQUIRE(query.conservativeQueryRadiusMeters < 10000.0);
    REQUIRE(query.subjectResidualSpeedMetersPerSecond <= 10.0 + 1e-9);
    REQUIRE(query.maxAnchorResidualSpeedMetersPerSecond <= 10.0 + 1e-9);
    REQUIRE(query.candidateIndices.size() == 1);
    REQUIRE(query.candidateIndices.front() == 0);
}

void testSpatialBroadphaseKeepsFastFutureCollisionCandidate()
{
    using namespace game::simulation::activation;

    KinematicPoint subject;
    subject.positionMeters = {-10000.0, 0.0, 0.0};
    subject.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    subject.bounds = SpatialBounds{18.0};

    ActivationAnchor target;
    target.id = EntityId{2};
    target.kind = ActivationAnchorKind::Ship;
    target.systemId = 0;
    target.kinematics.positionMeters = {10000.0, 0.0, 0.0};
    target.kinematics.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    target.kinematics.bounds = SpatialBounds{18.0};

    std::vector<ActivationAnchor> anchors{target};

    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;

    ActivationSpatialIndexConfig config;
    config.cellSizeMeters = 5000.0;
    ActivationSpatialIndex index(config);
    index.rebuild(anchors);

    const auto query = index.query(0, subject, policy);
    REQUIRE(!query.candidateIndices.empty());

    const auto decision = evaluateActivationShadowCandidates(
        EntityId{1},
        0,
        subject,
        SimulationMode::Active,
        false,
        anchors,
        query.candidateIndices,
        policy,
        query.usedFallback
    );

    REQUIRE(decision.desiredMode == SimulationMode::Prewarm);
    REQUIRE(decision.anchorId == target.id);
}

void testSpatialBroadphaseAccountsForLargeAnchorBounds()
{
    using namespace game::simulation::activation;

    KinematicPoint subject;
    subject.bounds = SpatialBounds{18.0};

    ActivationAnchor station;
    station.id = EntityId{9};
    station.kind = ActivationAnchorKind::StaticObject;
    station.systemId = 0;
    station.kinematics.positionMeters = {3000.0, 0.0, 0.0};
    station.kinematics.bounds = SpatialBounds{3800.0};

    std::vector<ActivationAnchor> anchors{station};
    ActivationSpatialIndex index;
    index.rebuild(anchors);

    const auto query = index.query(0, subject, InteractionHorizonPolicy{});
    REQUIRE(query.candidateIndices.size() == 1);

    const auto decision = evaluateActivationShadowCandidates(
        EntityId{1},
        0,
        subject,
        SimulationMode::Active,
        false,
        anchors,
        query.candidateIndices,
        InteractionHorizonPolicy{},
        query.usedFallback
    );

    REQUIRE(decision.desiredMode == SimulationMode::Active);
    REQUIRE(decision.anchorId == station.id);
}

void testSpatialBroadphaseFallbackPreservesSameSystemCorrectness()
{
    using namespace game::simulation::activation;

    ActivationAnchor sameSystem;
    sameSystem.id = EntityId{2};
    sameSystem.kind = ActivationAnchorKind::Ship;
    sameSystem.systemId = 0;
    sameSystem.kinematics.positionMeters = {100.0, 0.0, 0.0};
    sameSystem.kinematics.bounds = SpatialBounds{18.0};

    ActivationAnchor otherSystem = sameSystem;
    otherSystem.id = EntityId{3};
    otherSystem.systemId = 1;

    std::vector<ActivationAnchor> anchors{sameSystem, otherSystem};

    ActivationSpatialIndexConfig config;
    config.cellSizeMeters = 1.0;
    config.maxVisitedCellsPerQuery = 8;

    ActivationSpatialIndex index(config);
    index.rebuild(anchors);

    KinematicPoint subject;
    subject.bounds = SpatialBounds{18.0};

    InteractionHorizonPolicy policy;
    policy.gameplayRangeMeters = 1000.0;

    const auto query = index.query(0, subject, policy);
    REQUIRE(query.usedFallback);
    REQUIRE(query.candidateIndices.size() == 1);
    REQUIRE(query.candidateIndices.front() == 0);
}


void testActiveNpcAiCadenceRunsEveryFrame()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;
    ActivationCadenceState state;

    for (int frame = 1; frame <= 10; ++frame)
    {
        const auto decision = advanceNpcAiCadence(
            state,
            SimulationMode::Active,
            0.02,
            frame * 0.02,
            policy
        );

        REQUIRE(decision.execute);
        REQUIRE(approx(decision.intervalSeconds, 0.0));
        REQUIRE(approx(decision.thinkDeltaSeconds, 0.02));
    }

    REQUIRE(state.executionCount == 10);
    REQUIRE(state.skippedFrameCount == 0);
}

void testPrewarmNpcAiCadenceDecimatesFixedTicks()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;
    ActivationCadenceState state;

    int executions = 0;
    for (int frame = 1; frame <= 50; ++frame)
    {
        const auto decision = advanceNpcAiCadence(
            state,
            SimulationMode::Prewarm,
            0.02,
            frame * 0.02,
            policy
        );

        executions += decision.execute ? 1 : 0;
    }

    REQUIRE(executions == 10);
    REQUIRE(state.executionCount == 10);
    REQUIRE(state.skippedFrameCount == 40);
}

void testCoarseNpcAiCadenceRunsAtOneHertz()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;
    ActivationCadenceState state;

    int executions = 0;
    for (int frame = 1; frame <= 150; ++frame)
    {
        const auto decision = advanceNpcAiCadence(
            state,
            SimulationMode::Coarse,
            0.02,
            frame * 0.02,
            policy
        );

        executions += decision.execute ? 1 : 0;
    }

    REQUIRE(executions == 3);
    REQUIRE(state.executionCount == 3);
    REQUIRE(state.skippedFrameCount == 147);
}

void testPromotionToActiveRunsNpcAiImmediately()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;
    ActivationCadenceState state;

    for (int frame = 1; frame <= 20; ++frame)
    {
        const auto coarse = advanceNpcAiCadence(
            state,
            SimulationMode::Coarse,
            0.02,
            frame * 0.02,
            policy
        );
        REQUIRE(!coarse.execute);
    }

    const auto active = advanceNpcAiCadence(
        state,
        SimulationMode::Active,
        0.02,
        0.42,
        policy
    );

    REQUIRE(active.execute);
    REQUIRE(active.thinkDeltaSeconds > 0.40);
    REQUIRE(active.thinkDeltaSeconds <= policy.maxNpcAiThinkDeltaSeconds);
}

void testNonMaterializedModesDoNotRunTacticalNpcAi()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;

    for (const auto mode : {
        SimulationMode::Scheduled,
        SimulationMode::OnDemand,
        SimulationMode::Dormant
    })
    {
        ActivationCadenceState state;
        const auto decision = advanceNpcAiCadence(
            state,
            mode,
            10.0,
            10.0,
            policy
        );

        REQUIRE(!decision.execute);
        REQUIRE(!std::isfinite(decision.intervalSeconds));
        REQUIRE(state.executionCount == 0);
        REQUIRE(state.skippedFrameCount == 1);
    }
}



void testShipMotionControlCadenceDecimatesControlSolver()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;

    const auto countExecutions = [&](SimulationMode mode)
    {
        ActivationCadenceState state;
        int executions = 0;

        for (int frame = 1; frame <= 50; ++frame)
        {
            const auto decision = advanceShipMotionControlCadence(
                state,
                mode,
                0.02,
                frame * 0.02,
                policy
            );
            executions += decision.execute ? 1 : 0;
        }

        return executions;
    };

    REQUIRE(countExecutions(SimulationMode::Active) == 50);
    REQUIRE(countExecutions(SimulationMode::Prewarm) == 25);
    REQUIRE(countExecutions(SimulationMode::Coarse) == 5);
}

void testCoarseMotionControlCadenceCarriesBoundedElapsedTime()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;
    ActivationCadenceState state;
    ActivationCadenceDecision decision;

    for (int frame = 1; frame <= 10; ++frame)
    {
        decision = advanceShipMotionControlCadence(
            state,
            SimulationMode::Coarse,
            0.02,
            frame * 0.02,
            policy
        );
    }

    REQUIRE(decision.execute);
    REQUIRE(approx(decision.intervalSeconds, 0.20));
    REQUIRE(approx(decision.executionDeltaSeconds, 0.20));
    REQUIRE(state.executionCount == 1);
    REQUIRE(state.skippedFrameCount == 9);
}

void testShipSystemsCadenceDecimatesMaterializedServiceWork()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;

    const auto countExecutions = [&](SimulationMode mode)
    {
        ActivationCadenceState state;
        int executions = 0;

        for (int frame = 1; frame <= 50; ++frame)
        {
            const auto decision = advanceShipSystemsCadence(
                state,
                mode,
                0.02,
                frame * 0.02,
                policy
            );
            executions += decision.execute ? 1 : 0;
        }

        return executions;
    };

    REQUIRE(countExecutions(SimulationMode::Active) == 50);
    REQUIRE(countExecutions(SimulationMode::Prewarm) == 10);
    REQUIRE(countExecutions(SimulationMode::Coarse) == 1);
}

void testShipMaintenanceCadenceDecimatesMaterializedMaintenance()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;

    const auto countExecutions = [&](SimulationMode mode)
    {
        ActivationCadenceState state;
        int executions = 0;

        for (int frame = 1; frame <= 50; ++frame)
        {
            const auto decision = advanceShipMaintenanceCadence(
                state,
                mode,
                0.02,
                frame * 0.02,
                policy
            );
            executions += decision.execute ? 1 : 0;
        }

        return executions;
    };

    REQUIRE(countExecutions(SimulationMode::Active) == 50);
    REQUIRE(countExecutions(SimulationMode::Prewarm) == 10);
    REQUIRE(countExecutions(SimulationMode::Coarse) == 1);
}

void testCoarseServiceCadenceCarriesElapsedTimeIntoExecution()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;
    ActivationCadenceState state;
    ActivationCadenceDecision decision;

    for (int frame = 1; frame <= 50; ++frame)
    {
        decision = advanceShipSystemsCadence(
            state,
            SimulationMode::Coarse,
            0.02,
            frame * 0.02,
            policy
        );
    }

    REQUIRE(decision.execute);
    REQUIRE(approx(decision.intervalSeconds, 1.0));
    REQUIRE(approx(decision.executionDeltaSeconds, 1.0));
    REQUIRE(state.executionCount == 1);
    REQUIRE(state.skippedFrameCount == 49);
}

void testNonMaterializedModesSkipServiceAndMaintenanceLanes()
{
    using namespace game::simulation::activation;

    ActivationExecutionPolicy policy;

    for (const auto mode : {
        SimulationMode::Scheduled,
        SimulationMode::OnDemand,
        SimulationMode::Dormant
    })
    {
        ActivationCadenceState motionState;
        ActivationCadenceState systemsState;
        ActivationCadenceState maintenanceState;

        const auto motion = advanceShipMotionControlCadence(
            motionState,
            mode,
            10.0,
            10.0,
            policy
        );
        const auto systems = advanceShipSystemsCadence(
            systemsState,
            mode,
            10.0,
            10.0,
            policy
        );
        const auto maintenance = advanceShipMaintenanceCadence(
            maintenanceState,
            mode,
            10.0,
            10.0,
            policy
        );

        REQUIRE(!motion.execute);
        REQUIRE(!systems.execute);
        REQUIRE(!maintenance.execute);
        REQUIRE(!std::isfinite(motion.intervalSeconds));
        REQUIRE(!std::isfinite(systems.intervalSeconds));
        REQUIRE(!std::isfinite(maintenance.intervalSeconds));
        REQUIRE(motionState.executionCount == 0);
        REQUIRE(systemsState.executionCount == 0);
        REQUIRE(maintenanceState.executionCount == 0);
    }
}

void testTransitionTelemetrySurvivesBetweenDiagnosticSamples()
{
    using namespace game::simulation::activation;

    ActivationPlanState state;
    state.plannedMode = SimulationMode::Active;
    state.releaseNotBeforeServerTimeSeconds = 2.0;

    ActivationHysteresisPolicy policy;
    policy.activeReleaseDelaySeconds = 2.0;
    policy.prewarmReleaseDelaySeconds = 1.0;

    const auto demote =
        updateActivationPlan(state, SimulationMode::Coarse, 2.1, policy);
    REQUIRE(demote.transition == ActivationPlanTransition::DemoteToPrewarm);
    REQUIRE(demote.transitionSerial == 1);
    REQUIRE(demote.lastTransition == ActivationPlanTransition::DemoteToPrewarm);
    REQUIRE(approx(demote.lastTransitionServerTimeSeconds, 2.1));

    const auto laterSample =
        updateActivationPlan(state, SimulationMode::Coarse, 2.5, policy);
    REQUIRE(laterSample.transition == ActivationPlanTransition::None);
    REQUIRE(laterSample.transitionSerial == 1);
    REQUIRE(
        laterSample.lastTransition ==
        ActivationPlanTransition::DemoteToPrewarm
    );
    REQUIRE(approx(laterSample.lastTransitionServerTimeSeconds, 2.1));

    const auto coarse =
        updateActivationPlan(state, SimulationMode::Coarse, 3.2, policy);
    REQUIRE(coarse.transition == ActivationPlanTransition::DemoteToCoarse);
    REQUIRE(coarse.transitionSerial == 2);
    REQUIRE(coarse.lastTransition == ActivationPlanTransition::DemoteToCoarse);
    REQUIRE(approx(coarse.lastTransitionServerTimeSeconds, 3.2));
}


void testActivationCadenceLabDemandSchedule()
{
    using game::diagnostics::activationCadenceLabDemand;

    const auto coarse = activationCadenceLabDemand(3.0);
    REQUIRE(!coarse.hasClaim);
    REQUIRE(coarse.minimumMode == SimulationMode::Coarse);
    REQUIRE(std::string(coarse.phase) == "coarse");

    const auto prewarm = activationCadenceLabDemand(8.0);
    REQUIRE(prewarm.hasClaim);
    REQUIRE(prewarm.minimumMode == SimulationMode::Prewarm);
    REQUIRE(std::string(prewarm.phase) == "prewarm-claim");

    const auto active = activationCadenceLabDemand(14.0);
    REQUIRE(active.hasClaim);
    REQUIRE(active.minimumMode == SimulationMode::Active);
    REQUIRE(std::string(active.phase) == "active-claim");

    const auto release = activationCadenceLabDemand(20.0);
    REQUIRE(!release.hasClaim);
    REQUIRE(release.minimumMode == SimulationMode::Coarse);
    REQUIRE(std::string(release.phase) == "release");

    const auto repeated = activationCadenceLabDemand(32.0);
    REQUIRE(repeated.hasClaim);
    REQUIRE(repeated.minimumMode == SimulationMode::Prewarm);
    REQUIRE(std::string(repeated.phase) == "prewarm-claim");
}

} // namespace

int main()
{
    int passed = 0;

    const auto run = [&](const char* name, auto&& test)
    {
        try
        {
            test();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        }
        catch (const std::exception& e)
        {
            std::cerr << "[FAIL] " << name << ": " << e.what() << '\n';
            throw;
        }
    };

    run("logical size produces different broad bounds", testLogicalSizeProducesDifferentBroadBounds);
    run("large station activates earlier than small ship", testLargeStationActivatesEarlierThanSmallShip);
    run("fast closing pair prewarms before range", testFastClosingPairPrewarmsBeforeDistanceThreshold);
    run("diverging pair stays asleep", testDivergingPairDoesNotPrewarm);
    run("short horizon does not wake too early", testShortHorizonDoesNotWakeFutureCollisionTooEarly);
    run("gameplay range remains separate from size", testGameplayRangeIsIndependentFromPhysicalRadius);
    run("player is pinned active in shadow mode", testPlayerIsPinnedActiveInShadowMode);
    run("large static anchor keeps ship active", testCurrentLargeStaticAnchorKeepsShipActive);
    run("future ship collision requests prewarm", testFutureShipCollisionRequestsPrewarm);
    run("no interaction requests coarse", testNoInteractionRequestsCoarse);
    run("other systems are ignored", testOtherSystemsDoNotAffectShadowDecision);
    run("active demotion uses hysteresis ladder", testActiveDemotionUsesHysteresisLadder);
    run("promotion is immediate", testPromotionIsImmediate);
    run("gameplay claim raises coarse to active", testGameplayClaimCanRaisePhysicalCoarseToActive);
    run("expired gameplay claim is ignored", testExpiredGameplayClaimDoesNotKeepEntityActive);
    run("claim cannot lower physical demand", testClaimCannotLowerPhysicalDemand);
    run("spatial broadphase prunes without changing decision", testSpatialBroadphasePrunesDistantAnchorsWithoutChangingDecision);
    run("spatial broadphase removes shared orbital bulk velocity", testSpatialBroadphaseRemovesSharedOrbitalBulkVelocity);
    run("spatial broadphase keeps fast future collision", testSpatialBroadphaseKeepsFastFutureCollisionCandidate);
    run("spatial broadphase accounts for large bounds", testSpatialBroadphaseAccountsForLargeAnchorBounds);
    run("spatial broadphase fallback preserves system correctness", testSpatialBroadphaseFallbackPreservesSameSystemCorrectness);
    run("active NPC AI cadence runs every frame", testActiveNpcAiCadenceRunsEveryFrame);
    run("prewarm NPC AI cadence decimates fixed ticks", testPrewarmNpcAiCadenceDecimatesFixedTicks);
    run("coarse NPC AI cadence runs at one hertz", testCoarseNpcAiCadenceRunsAtOneHertz);
    run("promotion to active runs NPC AI immediately", testPromotionToActiveRunsNpcAiImmediately);
    run("non-materialized modes skip tactical NPC AI", testNonMaterializedModesDoNotRunTacticalNpcAi);
    run("ship motion-control cadence decimates control solver", testShipMotionControlCadenceDecimatesControlSolver);
    run("coarse motion-control cadence carries bounded elapsed time", testCoarseMotionControlCadenceCarriesBoundedElapsedTime);
    run("ship systems cadence decimates service work", testShipSystemsCadenceDecimatesMaterializedServiceWork);
    run("ship maintenance cadence decimates maintenance", testShipMaintenanceCadenceDecimatesMaterializedMaintenance);
    run("coarse service cadence carries elapsed time", testCoarseServiceCadenceCarriesElapsedTimeIntoExecution);
    run("non-materialized modes skip service lanes", testNonMaterializedModesSkipServiceAndMaintenanceLanes);
    run("transition telemetry survives diagnostic sampling", testTransitionTelemetrySurvivesBetweenDiagnosticSamples);
    run("activation cadence lab demand schedule", testActivationCadenceLabDemandSchedule);

    std::cout << passed << "/34 tests passed\n";
    return passed == 34 ? 0 : 1;
}
