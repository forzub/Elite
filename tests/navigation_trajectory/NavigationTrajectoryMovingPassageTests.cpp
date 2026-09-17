#include "world/navigation/trajectory/MovingGapPredictor.h"
#include "world/navigation/trajectory/MovingPassageTrajectoryEvaluator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using GapPredictor = world::navigation::MovingGapPredictor;
using Evaluator = world::navigation::MovingPassageTrajectoryEvaluator;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(double actual, double expected, double tolerance, const std::string& message)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

GapPredictor::Query baseGapQuery()
{
    GapPredictor::Query query;
    query.travelDirectionMap = {0.0, 0.0, 1.0};
    query.primary.obstacleId = 1;
    query.primary.snapshotRevision = 11;
    query.primary.centerMapMeters = {-5.0, 0.0, 0.0};
    query.primary.conservativeRadiusMeters = 2.0;
    query.secondary.obstacleId = 2;
    query.secondary.snapshotRevision = 11;
    query.secondary.centerMapMeters = {5.0, 0.0, 0.0};
    query.secondary.conservativeRadiusMeters = 2.0;
    query.horizonSeconds = 1.0;
    query.policy.secondaryClearanceMeters = 20.0;
    query.policy.minimumContinuousClearSeparationMeters = 0.0;
    query.policy.maximumAbsSeparationTravelDot = 0.5;
    return query;
}

Evaluator::Query baseTrajectoryQuery(const GapPredictor::Result& gap)
{
    Evaluator::Query query;
    query.movingGap = &gap;
    query.hull.halfExtentsBodyMeters = {1.0, 1.0, 2.0};
    query.start.pose.centerMapMeters = {0.0, 0.0, -5.0};
    query.end.pose.centerMapMeters = {0.0, 0.0, 5.0};
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 10.0};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 10.0};
    query.durationSeconds = 1.0;
    query.linearCapability.maxForwardAccelerationMetersPerSec2 = 1000.0;
    query.linearCapability.maxReverseAccelerationMetersPerSec2 = 1000.0;
    query.linearCapability.maxLateralAccelerationMetersPerSec2 = 1000.0;
    query.linearCapability.maxVerticalAccelerationMetersPerSec2 = 1000.0;
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 1000.0;
    query.angularCapability.maxAngularSpeedRadPerSec = 1000.0;
    query.controlMode = Evaluator::ControlMode::Newtonian;
    return query;
}

void testStaticGapAndStraightShipSegmentAreFeasible()
{
    const GapPredictor::Result gap = GapPredictor::predict(baseGapQuery());
    const Evaluator::Result result = Evaluator::evaluate(baseTrajectoryQuery(gap));

    require(gap.status == GapPredictor::Status::OpenForHorizon,
            "static fixture requires an accepted moving-gap prediction");
    require(result.status == Evaluator::Status::Feasible,
            "straight ship segment must pass a static-in-time moving-gap result");
    require(result.feasible, "feasible result must set feasible=true");
    require(result.samplesEvaluated == Evaluator::kPoseSamples,
            "moving passage must evaluate all 33 ship poses");
    require(result.intervalsProven == Evaluator::kIntervals,
            "moving passage must prove all 32 continuous intervals");
    require(result.minimumContinuousClearanceBoundMeters > 1.9,
            "straight centered fixture must retain substantial width clearance");
}

void testShipCanTrackTranslatingGapCenter()
{
    GapPredictor::Query gapQuery = baseGapQuery();
    gapQuery.primary.linearVelocityMapMetersPerSec = {3.0, 0.0, 0.0};
    gapQuery.secondary.linearVelocityMapMetersPerSec = {3.0, 0.0, 0.0};
    const GapPredictor::Result gap = GapPredictor::predict(gapQuery);

    Evaluator::Query query = baseTrajectoryQuery(gap);
    query.start.linearVelocityMapMetersPerSec = {3.0, 0.0, 10.0};
    query.end.pose.centerMapMeters = {3.0, 0.0, 5.0};
    query.end.linearVelocityMapMetersPerSec = {3.0, 0.0, 10.0};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::Feasible,
            "ship matching a translating gap center must remain feasible");
    requireNear(result.maximumRelativeCenterMotionBoundMeters, 0.0, 1.0e-9,
                "co-moving centered fixture must have zero transverse relative motion");
}

void testShipThatDoesNotFollowMovingGapIsBlocked()
{
    GapPredictor::Query gapQuery = baseGapQuery();
    gapQuery.primary.linearVelocityMapMetersPerSec = {4.0, 0.0, 0.0};
    gapQuery.secondary.linearVelocityMapMetersPerSec = {4.0, 0.0, 0.0};
    const GapPredictor::Result gap = GapPredictor::predict(gapQuery);

    Evaluator::Query query = baseTrajectoryQuery(gap);
    const Evaluator::Result result = Evaluator::evaluate(query);

    require(result.status == Evaluator::Status::GeometryBlocked,
            "ship remaining behind while the gap translates away must be blocked");
    require(!result.feasible, "blocked moving passage must not claim feasibility");
}

void testBetweenSampleProofCanRejectWhileEverySampleStillFits()
{
    GapPredictor::Query gapQuery = baseGapQuery();
    gapQuery.primary.centerMapMeters = {-3.975, 0.0, 0.0};
    gapQuery.secondary.centerMapMeters = {3.975, 0.0, 0.0};
    const GapPredictor::Result gap = GapPredictor::predict(gapQuery);

    Evaluator::Query query = baseTrajectoryQuery(gap);
    query.hull.halfExtentsBodyMeters = {1.0, 0.5, 1.0};
    query.start.pose.centerMapMeters = {0.0, 0.0, 0.0};
    query.end.pose.centerMapMeters = {0.0, 0.0, 0.0};
    query.start.linearVelocityMapMetersPerSec = {10.0, 0.0, 0.0};
    query.end.linearVelocityMapMetersPerSec = {10.0, 0.0, 0.0};

    const Evaluator::Result result = Evaluator::evaluate(query);

    require(result.status == Evaluator::Status::GeometryBlocked,
            "continuous interval bound must reject an unproven between-sample excursion");
    require(result.minimumSampleClearanceMeters > 0.0,
            "pinned fixture requires every discrete ship pose to fit");
    require(result.minimumContinuousClearanceBoundMeters < 0.0,
            "continuous moving-passage bound must be stricter than point samples");
}

void testUnavailableMovingGapFailsBeforeShipProof()
{
    GapPredictor::Query gapQuery = baseGapQuery();
    gapQuery.primary.linearVelocityMapMetersPerSec = {4.0, 0.0, 0.0};
    gapQuery.secondary.linearVelocityMapMetersPerSec = {-4.0, 0.0, 0.0};
    const GapPredictor::Result gap = GapPredictor::predict(gapQuery);
    require(gap.status == GapPredictor::Status::GapClosesDuringHorizon,
            "fixture must close the gap before the horizon ends");

    const Evaluator::Result result = Evaluator::evaluate(baseTrajectoryQuery(gap));
    require(result.status == Evaluator::Status::GapUnavailable,
            "closed upstream gap must fail before trajectory geometry work");
    require(result.samplesEvaluated == 0,
            "unavailable gap must not evaluate ship samples");
}

void testLateralAccelerationStillUsesTruthfulVehicleAuthority()
{
    const GapPredictor::Result gap = GapPredictor::predict(baseGapQuery());
    Evaluator::Query query = baseTrajectoryQuery(gap);
    query.start.pose.centerMapMeters = {0.0, 0.0, -5.0};
    query.end.pose.centerMapMeters = {1.0, 0.0, 5.0};
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 10.0};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 10.0};
    query.linearCapability.maxLateralAccelerationMetersPerSec2 = 1.0;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::LinearAuthorityExceeded,
            "moving passage must not invent lateral thrust authority");
    require(result.requiredPeakLateralAccelerationMetersPerSec2 > 1.0,
            "authority fixture must expose the required lateral acceleration");
}

void testNewtonianAndAssistedSlipRemainDistinct()
{
    GapPredictor::Query gapQuery = baseGapQuery();
    gapQuery.primary.linearVelocityMapMetersPerSec = {2.0, 0.0, 0.0};
    gapQuery.secondary.linearVelocityMapMetersPerSec = {2.0, 0.0, 0.0};
    const GapPredictor::Result gap = GapPredictor::predict(gapQuery);

    Evaluator::Query newton = baseTrajectoryQuery(gap);
    newton.start.pose.centerMapMeters = {0.0, 0.0, 0.0};
    newton.end.pose.centerMapMeters = {2.0, 0.0, 0.0};
    newton.start.linearVelocityMapMetersPerSec = {2.0, 0.0, 0.0};
    newton.end.linearVelocityMapMetersPerSec = {2.0, 0.0, 0.0};
    newton.controlMode = Evaluator::ControlMode::Newtonian;

    const Evaluator::Result newtonResult = Evaluator::evaluate(newton);
    require(newtonResult.status == Evaluator::Status::Feasible,
            "Newtonian mode must allow velocity independent of hull forward");

    Evaluator::Query assisted = newton;
    assisted.controlMode = Evaluator::ControlMode::EliteAssisted;
    assisted.assistedMaxVelocityToForwardAngleRad = 0.25;
    const Evaluator::Result assistedResult = Evaluator::evaluate(assisted);
    require(assistedResult.status == Evaluator::Status::AssistedSlipExceeded,
            "assisted mode must enforce supplied velocity-to-forward policy");
}

} // namespace

int main()
{
    try
    {
        testStaticGapAndStraightShipSegmentAreFeasible();
        testShipCanTrackTranslatingGapCenter();
        testShipThatDoesNotFollowMovingGapIsBlocked();
        testBetweenSampleProofCanRejectWhileEverySampleStillFits();
        testUnavailableMovingGapFailsBeforeShipProof();
        testLateralAccelerationStillUsesTruthfulVehicleAuthority();
        testNewtonianAndAssistedSlipRemainDistinct();

        std::cout << "NAVIGATION TRAJECTORY MOVING PASSAGE TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY MOVING PASSAGE TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
