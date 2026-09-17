#include "world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h"
#include "world/navigation/trajectory/OrientedPassageEvaluator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Evaluator = world::navigation::ContinuousPassageTrajectoryEvaluator;
using PassageEval = world::navigation::OrientedPassageEvaluator;

constexpr double kPi = 3.141592653589793238462643383279502884;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Evaluator::Basis3d rolled90()
{
    Evaluator::Basis3d basis;
    basis.right = {0.0, 1.0, 0.0};
    basis.up = {-1.0, 0.0, 0.0};
    basis.forward = {0.0, 0.0, 1.0};
    return basis;
}

Evaluator::Passage zPassage(double halfWidth, double halfHeight)
{
    Evaluator::Passage passage;
    passage.centerMapMeters = {0.0, 0.0, 0.0};
    passage.halfWidthMeters = halfWidth;
    passage.halfHeightMeters = halfHeight;
    return passage;
}

Evaluator::Query straightZQuery()
{
    Evaluator::Query query;
    query.hull.halfExtentsBodyMeters = {1.0, 0.5, 2.0};
    query.passage = zPassage(2.0, 2.0);
    query.start.pose.centerMapMeters = {0.0, 0.0, -5.0};
    query.end.pose.centerMapMeters = {0.0, 0.0, 5.0};
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 5.0};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 5.0};
    query.durationSeconds = 2.0;
    query.controlMode = Evaluator::ControlMode::Newtonian;
    return query;
}

void testStraightAlignedSegmentIsContinuouslyFeasible()
{
    Evaluator::Query query = straightZQuery();
    const Evaluator::Result result = Evaluator::evaluate(query);

    require(result.status == Evaluator::Status::Feasible,
            "straight aligned passage must be feasible");
    require(result.feasible, "feasible segment must set feasible=true");
    require(result.samplesEvaluated == Evaluator::kPoseSamples,
            "all fixed trajectory samples must be evaluated");
    require(result.intervalsProven + 1 == Evaluator::kPoseSamples,
            "every inter-sample interval must receive a continuous proof");
    require(result.minimumContinuousClearanceBoundMeters > 0.0,
            "straight centered fixture must retain positive continuous clearance");
}

void testIntermediateRotationCanClipDespiteBothEndpointsFitting()
{
    Evaluator::Query query = straightZQuery();
    query.hull.halfExtentsBodyMeters = {3.0, 1.0, 1.0};
    query.passage = zPassage(3.05, 10.0);
    query.end.pose.bodyToMap = rolled90();
    query.durationSeconds = 4.0;
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 2.5};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 2.5};
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 10.0;
    query.angularCapability.maxAngularSpeedRadPerSec = 10.0;

    PassageEval::Pose startPose = query.start.pose;
    PassageEval::Pose endPose = query.end.pose;
    const auto startFit = PassageEval::evaluate(query.hull, startPose, query.passage);
    const auto endFit = PassageEval::evaluate(query.hull, endPose, query.passage);
    require(startFit.fits && endFit.fits,
            "fixture endpoints must both fit before testing the swept rotation");

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::GeometryBlocked,
            "intermediate oriented sweep must reject a wall-clipping rotation");
    require(!result.feasible,
            "endpoint fits must not hide an intermediate hull collision");
    require(result.minimumContinuousClearanceBoundMeters < 0.0 ||
                result.minimumSampleClearanceMeters < 0.0,
            "blocked sweep must expose a negative geometry margin");
}

void testWiderSlotAcceptsTheSameContinuousRoll()
{
    Evaluator::Query query = straightZQuery();
    query.hull.halfExtentsBodyMeters = {3.0, 1.0, 1.0};
    query.passage = zPassage(3.60, 10.0);
    query.end.pose.bodyToMap = rolled90();
    query.durationSeconds = 4.0;
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 2.5};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 2.5};
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 10.0;
    query.angularCapability.maxAngularSpeedRadPerSec = 10.0;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::Feasible,
            "wider slot must admit the same bounded roll trajectory");
    require(result.minimumContinuousClearanceBoundMeters > 0.0,
            "accepted rolled trajectory must keep positive conservative clearance");
}

void testLateralCorrectionRespectsBodyAxisAuthority()
{
    Evaluator::Query query = straightZQuery();
    query.passage = zPassage(20.0, 20.0);
    query.start.pose.centerMapMeters.x = 1.0;
    query.end.pose.centerMapMeters.x = 0.0;
    query.linearCapability.maxLateralAccelerationMetersPerSec2 = 1.0;

    const Evaluator::Result rejected = Evaluator::evaluate(query);
    require(rejected.status == Evaluator::Status::LinearAuthorityExceeded,
            "too-small lateral thruster authority must reject the Hermite correction");
    require(rejected.requiredPeakLateralAccelerationMetersPerSec2 > 1.0,
            "rejected correction must expose required lateral acceleration");

    query.linearCapability.maxLateralAccelerationMetersPerSec2 = 2.0;
    const Evaluator::Result accepted = Evaluator::evaluate(query);
    require(accepted.status == Evaluator::Status::Feasible,
            "sufficient lateral authority must accept the same correction");
}

void testAngularAuthorityIsCheckedAnalytically()
{
    Evaluator::Query query = straightZQuery();
    query.passage = zPassage(10.0, 10.0);
    query.end.pose.bodyToMap = rolled90();
    query.durationSeconds = 1.0;
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 10.0};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 10.0};
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 20.0;
    query.angularCapability.maxAngularSpeedRadPerSec = 1.0;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::AngularAuthorityExceeded,
            "90 degree one-second roll must exceed a 1 rad/s angular-speed limit");
    require(result.requiredPeakAngularSpeedRadPerSec > 2.0,
            "analytic smoothstep peak angular speed should exceed 2 rad/s");
}

Evaluator::Query sidewaysVelocityQuery(Evaluator::ControlMode mode)
{
    Evaluator::Query query;
    query.hull.halfExtentsBodyMeters = {0.5, 0.5, 0.5};
    query.passage.centerMapMeters = {0.0, 0.0, 0.0};
    query.passage.passageToMap.right = {0.0, 0.0, -1.0};
    query.passage.passageToMap.up = {0.0, 1.0, 0.0};
    query.passage.passageToMap.forward = {1.0, 0.0, 0.0};
    query.passage.halfWidthMeters = 5.0;
    query.passage.halfHeightMeters = 5.0;
    query.start.pose.centerMapMeters = {-5.0, 0.0, 0.0};
    query.end.pose.centerMapMeters = {5.0, 0.0, 0.0};
    query.start.linearVelocityMapMetersPerSec = {5.0, 0.0, 0.0};
    query.end.linearVelocityMapMetersPerSec = {5.0, 0.0, 0.0};
    query.durationSeconds = 2.0;
    query.controlMode = mode;
    query.assistedMaxVelocityToForwardAngleRad = kPi / 4.0;
    return query;
}

void testNewtonAllowsVelocityAttitudeDivergenceButElitePolicyCanRejectIt()
{
    const Evaluator::Result newton = Evaluator::evaluate(
        sidewaysVelocityQuery(Evaluator::ControlMode::Newtonian)
    );
    require(newton.status == Evaluator::Status::Feasible,
            "Newtonian mode must allow inertial velocity to differ from hull forward");

    const Evaluator::Result elite = Evaluator::evaluate(
        sidewaysVelocityQuery(Evaluator::ControlMode::EliteAssisted)
    );
    require(elite.status == Evaluator::Status::AssistedSlipExceeded,
            "assisted Elite policy must reject excessive velocity/forward slip");
    require(elite.maximumObservedAssistedSlipAngleRad > kPi / 3.0,
            "assisted fixture must expose a large measured slip angle");
}

void testInvalidDurationFailsClosed()
{
    Evaluator::Query query = straightZQuery();
    query.durationSeconds = 0.0;
    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::InvalidInput,
            "zero-duration trajectory must fail closed");
    require(!result.feasible, "invalid trajectory must never be feasible");
}

} // namespace

int main()
{
    try
    {
        testStraightAlignedSegmentIsContinuouslyFeasible();
        testIntermediateRotationCanClipDespiteBothEndpointsFitting();
        testWiderSlotAcceptsTheSameContinuousRoll();
        testLateralCorrectionRespectsBodyAxisAuthority();
        testAngularAuthorityIsCheckedAnalytically();
        testNewtonAllowsVelocityAttitudeDivergenceButElitePolicyCanRejectIt();
        testInvalidDurationFailsClosed();

        std::cout << "NAVIGATION TRAJECTORY CONTINUOUS PASSAGE TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY CONTINUOUS PASSAGE TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
