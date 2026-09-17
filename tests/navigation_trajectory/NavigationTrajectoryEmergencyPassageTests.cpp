#include "world/navigation/trajectory/EmergencyPassageMitigator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Mitigator = world::navigation::EmergencyPassageMitigator;
using PassageEvaluator = world::navigation::OrientedPassageEvaluator;

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kTolerance = 1.0e-8;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) <= kTolerance;
}

Mitigator::Basis3d rolled90()
{
    Mitigator::Basis3d basis;
    basis.right = {0.0, 1.0, 0.0};
    basis.up = {-1.0, 0.0, 0.0};
    basis.forward = {0.0, 0.0, 1.0};
    return basis;
}

Mitigator::HullProxy flatHull()
{
    Mitigator::HullProxy hull;
    hull.halfExtentsBodyMeters = {4.0, 1.0, 6.0};
    return hull;
}

Mitigator::Passage slot(double halfWidth, double halfHeight)
{
    Mitigator::Passage passage;
    passage.source = PassageEvaluator::PassageSource::ObstacleGap;
    passage.centerMapMeters = {0.0, 0.0, 20.0};
    passage.passageToMap = {};
    passage.halfWidthMeters = halfWidth;
    passage.halfHeightMeters = halfHeight;
    return passage;
}

Mitigator::Query baseQuery(double halfWidth = 1.5, double halfHeight = 5.0)
{
    Mitigator::Query query;
    query.hull = flatHull();
    query.passage = slot(halfWidth, halfHeight);
    query.predictedEntryCenterMapMeters = query.passage.centerMapMeters;
    query.currentBodyToMap = {};
    query.preferredBodyToMap = rolled90();
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 0.5 * kPi;
    query.angularCapability.maxAngularSpeedRadPerSec = 0.5 * kPi;
    query.closingSpeedMetersPerSec = 10.0;
    query.maxBrakingAccelerationMetersPerSec2 = 5.0;
    return query;
}

PassageEvaluator::Result currentPassage(const Mitigator::Query& query)
{
    PassageEvaluator::Pose pose;
    pose.centerMapMeters = query.predictedEntryCenterMapMeters;
    pose.bodyToMap = query.currentBodyToMap;
    return PassageEvaluator::evaluate(query.hull, pose, query.passage);
}

void testEnoughRoomProducesSafeReachableEntryPose()
{
    Mitigator::Query query = baseQuery();
    query.distanceToEntryMeters = 30.0;

    const Mitigator::Result result = Mitigator::evaluate(query);

    require(result.status == Mitigator::Status::SafeEntryPose,
            "enough distance must preserve a collision-free entry-pose candidate");
    require(result.commandValid && result.collisionFreeEntryPoseProven,
            "safe entry pose must remain an active navigation command");
    require(!result.contactExpected,
            "safe entry pose must not be mislabeled as expected contact");
    require(result.passageAtEntry.fits,
            "selected safe entry pose must geometrically fit");
}

void testPartialReachableRollCanFitEvenWhenPreferredRollIsTooLate()
{
    Mitigator::Query query = baseQuery(3.8, 5.0);
    query.distanceToEntryMeters = 9.0;

    const Mitigator::Result result = Mitigator::evaluate(query);

    require(result.status == Mitigator::Status::SafeEntryPose,
            "reachable partial roll should be accepted when it already fits");
    require(result.appliedCorrectionRad > 0.0 &&
            result.appliedCorrectionRad < 0.5 * kPi,
            "fixture must use a partial attitude rather than the unreachable full roll");
    require(result.passageAtEntry.fits,
            "partial attitude must actually fit the wider emergency slot");
}

void testUnreachableSafeRollStillProducesMitigatedContactCommand()
{
    Mitigator::Query query = baseQuery();
    query.distanceToEntryMeters = 9.0;

    const PassageEvaluator::Result before = currentPassage(query);
    const Mitigator::Result result = Mitigator::evaluate(query);

    require(result.status == Mitigator::Status::EmergencyMitigatedContact,
            "too-late narrow entry must become explicit mitigated-contact intent");
    require(result.commandValid,
            "unavoidable contact must not disable navigation/control intent");
    require(result.contactExpected && !result.collisionFreeEntryPoseProven,
            "emergency contact must never masquerade as a safe passage");
    require(result.maximumBrakingRecommended,
            "unavoidable contact should reduce entry energy when braking exists");
    require(result.appliedCorrectionRad > 0.0 &&
            result.appliedCorrectionRad < 0.5 * kPi,
            "ship must continue rotating as far as physically useful before impact");
    require(result.clearanceDeficitMeters <
                std::max(0.0, -before.widthClearanceMeters) +
                std::max(0.0, -before.heightClearanceMeters),
            "best-effort attitude must reduce geometric overlap versus doing nothing");
    require(result.estimatedEntrySpeedMetersPerSec < query.closingSpeedMetersPerSec,
            "maximum braking must reduce predicted contact speed");
}

void testEmergencyIntentTargetsGapCenterAndTravelAxis()
{
    Mitigator::Query query = baseQuery();
    query.distanceToEntryMeters = 9.0;

    const Mitigator::Result result = Mitigator::evaluate(query);

    require(near(result.recommendedAimPointMapMeters.x, 0.0) &&
            near(result.recommendedAimPointMapMeters.y, 0.0) &&
            near(result.recommendedAimPointMapMeters.z, 20.0),
            "emergency intent must aim at the gap center");
    require(near(result.desiredTravelDirectionMap.x, 0.0) &&
            near(result.desiredTravelDirectionMap.y, 0.0) &&
            near(result.desiredTravelDirectionMap.z, 1.0),
            "emergency intent must align travel with the passage axis");
}

void testIfShipCanStopItDoesNotIntentionallyCrash()
{
    Mitigator::Query query = baseQuery(0.5, 0.5);
    query.distanceToEntryMeters = 15.0;

    const Mitigator::Result result = Mitigator::evaluate(query);

    require(result.status == Mitigator::Status::EmergencyStopBeforeEntry,
            "when no sampled attitude fits but stopping is possible, stop before contact");
    require(result.commandValid && result.canStopBeforeEntry,
            "emergency stop is an active navigation command, not planner shutdown");
    require(!result.contactExpected,
            "system must not intentionally collide when it can physically stop first");
    require(result.maximumBrakingRecommended,
            "stop-before-entry fixture must command maximum useful braking");
    require(near(result.estimatedEntrySpeedMetersPerSec, 0.0),
            "stop-before-entry must predict zero passage-entry speed");
}

void testZeroAngularAuthorityStillKeepsEmergencyCommandAlive()
{
    Mitigator::Query query = baseQuery();
    query.distanceToEntryMeters = 9.0;
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 0.0;
    query.angularCapability.maxAngularSpeedRadPerSec = 0.0;

    const Mitigator::Result result = Mitigator::evaluate(query);

    require(result.status == Mitigator::Status::EmergencyMitigatedContact,
            "zero rotation authority must still return best available contact mitigation");
    require(result.commandValid && result.contactExpected,
            "lack of rotation authority must not turn into no-navigation-command");
    require(near(result.appliedCorrectionRad, 0.0),
            "zero angular authority must not invent a hull rotation");
    require(result.maximumBrakingRecommended,
            "braking remains useful even when hull rotation is unavailable");
}

} // namespace

int main()
{
    try
    {
        testEnoughRoomProducesSafeReachableEntryPose();
        testPartialReachableRollCanFitEvenWhenPreferredRollIsTooLate();
        testUnreachableSafeRollStillProducesMitigatedContactCommand();
        testEmergencyIntentTargetsGapCenterAndTravelAxis();
        testIfShipCanStopItDoesNotIntentionallyCrash();
        testZeroAngularAuthorityStillKeepsEmergencyCommandAlive();

        std::cout << "NAVIGATION TRAJECTORY EMERGENCY PASSAGE TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY EMERGENCY PASSAGE TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
