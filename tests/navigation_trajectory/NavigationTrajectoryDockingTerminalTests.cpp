#include "world/navigation/trajectory/DockingTerminalEvaluator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Evaluator = world::navigation::DockingTerminalEvaluator;
using Vec3d = Evaluator::Vec3d;
using Basis3d = Evaluator::Basis3d;

constexpr double kPi = 3.141592653589793238462643383279502884;

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

Basis3d bottomToBottomShipBasis()
{
    Basis3d basis;
    basis.right = {-1.0, 0.0, 0.0};
    basis.up = {0.0, -1.0, 0.0};
    basis.forward = {0.0, 0.0, 1.0};
    return basis;
}

Evaluator::Query baseQuery()
{
    Evaluator::Query query;

    query.shipPort.surface = Evaluator::SurfaceSemantic::Bottom;
    query.shipPort.matingNormalLocal = {0.0, -1.0, 0.0};
    query.shipPort.referenceUpLocal = {0.0, 0.0, 1.0};
    query.shipAtCapture.pose.bodyToMap = bottomToBottomShipBasis();

    query.dockPort.surface = Evaluator::SurfaceSemantic::Bottom;
    query.dockPort.matingNormalLocal = {0.0, -1.0, 0.0};
    query.dockPort.referenceUpLocal = {0.0, 0.0, 1.0};

    query.contract.requiredShipSurface = Evaluator::SurfaceSemantic::Bottom;
    query.contract.requiredDockSurface = Evaluator::SurfaceSemantic::Bottom;
    query.contract.maxPositionErrorMeters = 0.05;
    query.contract.maxRelativeLinearSpeedMetersPerSec = 0.05;
    query.contract.maxNormalAlignmentErrorRad = 0.01;
    query.contract.maxRollAlignmentErrorRad = 0.01;
    query.contract.maxRelativeAngularSpeedRadPerSec = 0.01;
    return query;
}

void testStationaryBottomToBottomCapture()
{
    const Evaluator::Result result = Evaluator::evaluate(baseQuery());

    require(result.status == Evaluator::Status::Capturable,
            "aligned stationary bottom-to-bottom ports must capture");
    require(result.capturable, "Capturable must set capturable=true");
    requireNear(result.positionErrorMeters, 0.0, 1.0e-12,
                "stationary aligned port position error must be zero");
    requireNear(result.relativeLinearSpeedMetersPerSec, 0.0, 1.0e-12,
                "stationary aligned port relative speed must be zero");
    requireNear(result.matingNormalAlignmentErrorRad, 0.0, 1.0e-12,
                "bottom-to-bottom faces must be anti-aligned at capture");
    requireNear(result.rollAlignmentErrorRad, 0.0, 1.0e-12,
                "bottom-to-bottom reference-up must align at capture");
}

void testExplicitPortSemanticsAreEnforced()
{
    Evaluator::Query query = baseQuery();
    query.shipPort.surface = Evaluator::SurfaceSemantic::Top;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::PortSemanticMismatch,
            "top ship port must not satisfy an explicit bottom-to-bottom contract");
}

void testTranslatingDockRequiresVelocityMatchAtFutureCapture()
{
    Evaluator::Query query = baseQuery();
    query.captureTimeSeconds = 2.0;
    query.dock.originLinearVelocityMapMetersPerSec = {3.0, 0.0, 0.0};
    query.shipAtCapture.pose.centerMapMeters = {6.0, 0.0, 0.0};
    query.shipAtCapture.linearVelocityMapMetersPerSec = {3.0, 0.0, 0.0};

    const Evaluator::Result matched = Evaluator::evaluate(query);
    require(matched.status == Evaluator::Status::Capturable,
            "ship matching translating dock position and velocity must capture");

    query.shipAtCapture.linearVelocityMapMetersPerSec = {};
    const Evaluator::Result mismatched = Evaluator::evaluate(query);
    require(mismatched.status == Evaluator::Status::RelativeLinearVelocityMismatch,
            "same-position ship must fail capture when carrier velocity is unmatched");
    requireNear(mismatched.relativeLinearSpeedMetersPerSec, 3.0, 1.0e-9,
                "translation mismatch must report carrier-relative speed");
}

void testRotatingOffsetPortPublishesOmegaCrossRVelocity()
{
    Evaluator::Query query = baseQuery();
    query.dockPort.positionLocalMeters = {5.0, 0.0, 0.0};
    query.dock.angularVelocityMapRadPerSec = {0.0, 0.0, 1.0};

    query.shipAtCapture.pose.centerMapMeters = {5.0, 0.0, 0.0};
    query.shipAtCapture.linearVelocityMapMetersPerSec = {0.0, 5.0, 0.0};
    query.shipAtCapture.angularVelocityMapRadPerSec = {0.0, 0.0, 1.0};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::Capturable,
            "ship matching a rotating offset port must capture");
    requireNear(result.dockPortWorld.linearVelocityMapMetersPerSec.x, 0.0, 1.0e-9,
                "omega cross r x component must match fixture");
    requireNear(result.dockPortWorld.linearVelocityMapMetersPerSec.y, 5.0, 1.0e-9,
                "rotating offset port must include omega cross r tangential velocity");
}

void testCombinedMovingRotatingFutureDockFrame()
{
    Evaluator::Query query = baseQuery();
    query.captureTimeSeconds = 0.5 * kPi;
    query.dock.originLinearVelocityMapMetersPerSec = {1.0, 0.0, 0.0};
    query.dock.angularVelocityMapRadPerSec = {0.0, 0.0, 1.0};
    query.dockPort.positionLocalMeters = {2.0, 0.0, 0.0};

    // After +90 degrees about map Z, the dock body basis is:
    // right=+Y, up=-X, forward=+Z. Its bottom normal therefore points +X.
    // The mating ship bottom must point -X while both roll references stay +Z.
    query.shipAtCapture.pose.bodyToMap.right = {0.0, -1.0, 0.0};
    query.shipAtCapture.pose.bodyToMap.up = {1.0, 0.0, 0.0};
    query.shipAtCapture.pose.bodyToMap.forward = {0.0, 0.0, 1.0};

    query.shipAtCapture.pose.centerMapMeters = {0.5 * kPi, 2.0, 0.0};
    query.shipAtCapture.linearVelocityMapMetersPerSec = {-1.0, 0.0, 0.0};
    query.shipAtCapture.angularVelocityMapRadPerSec = {0.0, 0.0, 1.0};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::Capturable,
            "future combined translating/rotating terminal frame must be capturable when matched");
    requireNear(result.positionErrorMeters, 0.0, 1.0e-9,
                "future moving/rotating port position must be predicted at capture time");
    requireNear(result.relativeLinearSpeedMetersPerSec, 0.0, 1.0e-9,
                "future moving/rotating port velocity must be matched");
    requireNear(result.relativeAngularSpeedRadPerSec, 0.0, 1.0e-9,
                "future rotating port angular rate must be matched");
}

void testOneEightyDegreeRolledApproachIsRejected()
{
    Evaluator::Query query = baseQuery();

    // This still maps ship bottom normal -Y to +Y, correctly opposing the dock
    // bottom normal -Y, but flips the port reference-up +Z to -Z.
    query.shipAtCapture.pose.bodyToMap.right = {1.0, 0.0, 0.0};
    query.shipAtCapture.pose.bodyToMap.up = {0.0, -1.0, 0.0};
    query.shipAtCapture.pose.bodyToMap.forward = {0.0, 0.0, -1.0};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::RollAlignmentMismatch,
            "180-degree rolled approach must fail even when mating normals are correct");
    requireNear(result.matingNormalAlignmentErrorRad, 0.0, 1.0e-12,
                "rolled rejection fixture must preserve correct face-normal alignment");
    requireNear(result.rollAlignmentErrorRad, kPi, 1.0e-9,
                "rolled rejection fixture must expose pi roll error");
}

void testRelativeAngularRateIsCaptureGate()
{
    Evaluator::Query query = baseQuery();
    query.dock.angularVelocityMapRadPerSec = {0.0, 0.0, 0.2};
    query.shipAtCapture.angularVelocityMapRadPerSec = {};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::RelativeAngularVelocityMismatch,
            "matching pose without matching angular rate must not capture");
    requireNear(result.relativeAngularSpeedRadPerSec, 0.2, 1.0e-9,
                "angular-rate gate must report relative omega magnitude");
}

void testPositionToleranceIsCaptureGate()
{
    Evaluator::Query query = baseQuery();
    query.shipAtCapture.pose.centerMapMeters = {0.1, 0.0, 0.0};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::PositionMismatch,
            "terminal position outside tolerance must fail capture");
}

} // namespace

int main()
{
    try
    {
        testStationaryBottomToBottomCapture();
        testExplicitPortSemanticsAreEnforced();
        testTranslatingDockRequiresVelocityMatchAtFutureCapture();
        testRotatingOffsetPortPublishesOmegaCrossRVelocity();
        testCombinedMovingRotatingFutureDockFrame();
        testOneEightyDegreeRolledApproachIsRejected();
        testRelativeAngularRateIsCaptureGate();
        testPositionToleranceIsCaptureGate();

        std::cout << "NAVIGATION TRAJECTORY DOCKING TERMINAL TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY DOCKING TERMINAL TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
