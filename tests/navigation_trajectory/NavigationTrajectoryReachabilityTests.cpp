#include "world/navigation/trajectory/AttitudeReachabilityEvaluator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Reachability = world::navigation::AttitudeReachabilityEvaluator;

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kTolerance = 1.0e-9;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) <= kTolerance;
}

Reachability::Basis3d rolled90()
{
    Reachability::Basis3d basis;
    basis.right = {0.0, 1.0, 0.0};
    basis.up = {-1.0, 0.0, 0.0};
    basis.forward = {0.0, 0.0, 1.0};
    return basis;
}

Reachability::AngularCapability capability()
{
    Reachability::AngularCapability value;
    value.maxAngularAccelerationRadPerSec2 = 0.5 * kPi;
    value.maxAngularSpeedRadPerSec = 0.5 * kPi;
    return value;
}

Reachability::ApproachState baseApproach()
{
    Reachability::ApproachState approach;
    approach.requiredBodyToMap = rolled90();
    approach.closingSpeedMetersPerSec = 10.0;
    approach.maxBrakingAccelerationMetersPerSec2 = 5.0;
    return approach;
}

void testAlreadyAlignedIsReadyImmediately()
{
    Reachability::ApproachState approach;
    approach.distanceToEntryMeters = 1.0;
    approach.closingSpeedMetersPerSec = 100.0;

    const Reachability::Result result = Reachability::evaluate(
        capability(),
        approach
    );

    require(result.status == Reachability::Status::AlreadyReady,
            "aligned non-rotating hull must already be attitude-ready");
    require(result.reachable, "already-ready hull must be reachable");
    require(near(result.minimumRotationTimeSeconds, 0.0),
            "already-ready fixture must require zero rotation time");
}

void testNinetyDegreeRollFitsWithCoastWhenDistanceIsEnough()
{
    Reachability::ApproachState approach = baseApproach();
    approach.distanceToEntryMeters = 30.0;

    const Reachability::Result result = Reachability::evaluate(
        capability(),
        approach
    );

    require(result.status == Reachability::Status::ReachableCoast,
            "90 degree roll must be coast-reachable with 30 m available");
    require(result.reachable && !result.brakingRequired,
            "coast-reachable fixture must not require braking");
    require(near(result.attitudeErrorRad, 0.5 * kPi),
            "90 degree roll attitude error mismatch");
    require(near(result.minimumRotationTimeSeconds, 2.0),
            "90 degree triangular rest-to-rest turn should take 2 seconds");
    require(near(result.coastDistanceBeforeReadyMeters, 20.0),
            "coast distance before attitude ready mismatch");
    require(near(result.distanceMarginMeters, 10.0),
            "coast reachability margin mismatch");
}

void testNinetyDegreeRollCanRequireBraking()
{
    Reachability::ApproachState approach = baseApproach();
    approach.distanceToEntryMeters = 15.0;

    const Reachability::Result result = Reachability::evaluate(
        capability(),
        approach
    );

    require(result.status == Reachability::Status::ReachableWithBraking,
            "15 m fixture must require braking while rolling");
    require(result.reachable && result.brakingRequired,
            "braking-assisted fixture must be reachable and flag braking");
    require(near(result.coastDistanceBeforeReadyMeters, 20.0),
            "coast distance must still expose why braking is necessary");
    require(near(result.minimumDistanceBeforeReadyMeters, 10.0),
            "maximum braking should reduce 2 s approach travel to 10 m");
    require(near(result.distanceMarginMeters, 5.0),
            "braking-assisted margin mismatch");
}

void testHighSpeedShortDistanceMakesGapUnreachable()
{
    Reachability::ApproachState approach = baseApproach();
    approach.distanceToEntryMeters = 8.0;

    const Reachability::Result result = Reachability::evaluate(
        capability(),
        approach
    );

    require(result.status == Reachability::Status::UnreachableBeforeEntry,
            "8 m fixture must be unreachable before the required roll is ready");
    require(!result.reachable,
            "short-distance high-speed fixture must fail closed");
    require(near(result.minimumDistanceBeforeReadyMeters, 10.0),
            "unreachable fixture must expose the physical 10 m minimum");
    require(result.distanceMarginMeters < 0.0,
            "unreachable fixture must report negative distance margin");
}

void testCurrentAngularMotionConsumesAdditionalMargin()
{
    Reachability::ApproachState approach = baseApproach();
    approach.distanceToEntryMeters = 20.0;
    approach.currentAngularVelocityMapRadPerSec = {0.0, 0.0, 0.5 * kPi};

    const Reachability::Result result = Reachability::evaluate(
        capability(),
        approach
    );

    require(result.angularSettleTimeSeconds > 0.9,
            "current angular motion must add settle time");
    require(result.conservativeSettleAngleRad > 0.7,
            "current angular motion must add conservative attitude drift");
    require(result.minimumRotationTimeSeconds > 2.0,
            "nonzero angular motion must not be treated as free alignment");
}

void testZeroAngularAuthorityFailsClosed()
{
    Reachability::AngularCapability invalid = capability();
    invalid.maxAngularAccelerationRadPerSec2 = 0.0;

    Reachability::ApproachState approach = baseApproach();
    approach.distanceToEntryMeters = 100.0;

    const Reachability::Result result = Reachability::evaluate(
        invalid,
        approach
    );

    require(result.status == Reachability::Status::InvalidInput,
            "zero angular authority must fail closed");
    require(!result.reachable,
            "invalid angular capability must never claim reachability");
}

} // namespace

int main()
{
    try
    {
        testAlreadyAlignedIsReadyImmediately();
        testNinetyDegreeRollFitsWithCoastWhenDistanceIsEnough();
        testNinetyDegreeRollCanRequireBraking();
        testHighSpeedShortDistanceMakesGapUnreachable();
        testCurrentAngularMotionConsumesAdditionalMargin();
        testZeroAngularAuthorityFailsClosed();

        std::cout << "NAVIGATION TRAJECTORY ATTITUDE REACHABILITY TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY ATTITUDE REACHABILITY TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
