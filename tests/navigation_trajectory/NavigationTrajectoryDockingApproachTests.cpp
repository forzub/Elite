#include "world/navigation/trajectory/DockingApproachEvaluator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Evaluator = world::navigation::DockingApproachEvaluator;
using Terminal = Evaluator::TerminalEvaluator;
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

Basis3d bottomToBottomShipBasisRotatedPlus90Z()
{
    Basis3d basis;
    basis.right = {0.0, -1.0, 0.0};
    basis.up = {1.0, 0.0, 0.0};
    basis.forward = {0.0, 0.0, 1.0};
    return basis;
}

Basis3d bottomFaceButRolled180()
{
    Basis3d basis;
    basis.right = {1.0, 0.0, 0.0};
    basis.up = {0.0, -1.0, 0.0};
    basis.forward = {0.0, 0.0, -1.0};
    return basis;
}

Evaluator::Query baseQuery()
{
    Evaluator::Query query;
    query.hull.halfExtentsBodyMeters = {0.5, 0.5, 0.5};

    query.start.pose.centerMapMeters = {0.0, -5.0, 0.0};
    query.start.pose.bodyToMap = bottomToBottomShipBasis();
    query.start.linearVelocityMapMetersPerSec = {0.0, 5.0, 0.0};

    query.end.pose.centerMapMeters = {};
    query.end.pose.bodyToMap = bottomToBottomShipBasis();
    query.end.linearVelocityMapMetersPerSec = {};
    query.durationSeconds = 2.0;

    query.linearCapability.maxForwardAccelerationMetersPerSec2 = 1000.0;
    query.linearCapability.maxReverseAccelerationMetersPerSec2 = 1000.0;
    query.linearCapability.maxLateralAccelerationMetersPerSec2 = 1000.0;
    query.linearCapability.maxVerticalAccelerationMetersPerSec2 = 1000.0;
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 1000.0;
    query.angularCapability.maxAngularSpeedRadPerSec = 1000.0;
    query.controlMode = Evaluator::ControlMode::Newtonian;

    query.shipPort.surface = Terminal::SurfaceSemantic::Bottom;
    query.shipPort.matingNormalLocal = {0.0, -1.0, 0.0};
    query.shipPort.referenceUpLocal = {0.0, 0.0, 1.0};

    query.dockPort.surface = Terminal::SurfaceSemantic::Bottom;
    query.dockPort.matingNormalLocal = {0.0, -1.0, 0.0};
    query.dockPort.referenceUpLocal = {0.0, 0.0, 1.0};

    query.terminalContract.requiredShipSurface = Terminal::SurfaceSemantic::Bottom;
    query.terminalContract.requiredDockSurface = Terminal::SurfaceSemantic::Bottom;
    query.terminalContract.maxPositionErrorMeters = 0.05;
    query.terminalContract.maxRelativeLinearSpeedMetersPerSec = 0.05;
    query.terminalContract.maxNormalAlignmentErrorRad = 0.01;
    query.terminalContract.maxRollAlignmentErrorRad = 0.01;
    query.terminalContract.maxRelativeAngularSpeedRadPerSec = 0.01;

    query.corridor.halfWidthMeters = 2.0;
    query.corridor.halfHeightMeters = 2.0;
    return query;
}

void testStationaryDockFinalApproachIsContinuousAndCapturable()
{
    const Evaluator::Result result = Evaluator::evaluate(baseQuery());

    require(result.status == Evaluator::Status::FeasibleForCapture,
            "stationary final docking approach must be feasible");
    require(result.feasible, "FeasibleForCapture must set feasible=true");
    require(result.samplesEvaluated == Evaluator::kPoseSamples,
            "docking approach must evaluate all 33 physical samples");
    require(result.intervalsProven == Evaluator::kIntervals,
            "dock-local corridor must prove all 32 continuous intervals");
    require(result.dockLocalGeometry.status ==
                Evaluator::GeometryEvaluator::Status::Feasible,
            "stationary docking corridor geometry must be continuously feasible");
    require(result.terminal.status == Terminal::Status::Capturable,
            "successful approach must terminate in accepted 6DoF capture state");
}

void testTranslatingDockIsSolvedInRelativeFrame()
{
    Evaluator::Query query = baseQuery();
    query.dock.originLinearVelocityMapMetersPerSec = {2.0, 0.0, 0.0};
    query.start.linearVelocityMapMetersPerSec = {2.0, 5.0, 0.0};
    query.end.pose.centerMapMeters = {4.0, 0.0, 0.0};
    query.end.linearVelocityMapMetersPerSec = {2.0, 0.0, 0.0};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::FeasibleForCapture,
            "co-moving ship must retain the same feasible relative docking segment");
    requireNear(result.terminal.relativeLinearSpeedMetersPerSec, 0.0, 1.0e-9,
                "translating dock terminal relative velocity must converge to zero");
}

void testMovingRotatingDockEndsCoRotatingAtCapture()
{
    Evaluator::Query query = baseQuery();
    query.durationSeconds = 1.0;
    const double omega = 0.5 * kPi;
    query.dock.originLinearVelocityMapMetersPerSec = {1.0, 0.0, 0.0};
    query.dock.angularVelocityMapRadPerSec = {0.0, 0.0, omega};

    query.start.pose.centerMapMeters = {0.0, -5.0, 0.0};
    query.start.linearVelocityMapMetersPerSec = {
        1.0 + 5.0 * omega,
        5.0,
        0.0
    };
    query.start.pose.bodyToMap = bottomToBottomShipBasis();

    query.end.pose.centerMapMeters = {1.0, 0.0, 0.0};
    query.end.linearVelocityMapMetersPerSec = {1.0, 0.0, 0.0};
    query.end.pose.bodyToMap = bottomToBottomShipBasisRotatedPlus90Z();

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::FeasibleForCapture,
            "moving + rotating dock must be feasible when ship follows dock-local trajectory");
    requireNear(result.relativeOrientationChangeRad, 0.0, 1.0e-9,
                "co-rotating ship should need no relative attitude change in dock frame");
    requireNear(result.requiredPeakWorldAngularSpeedBoundRadPerSec,
                omega, 1.0e-9,
                "world angular-speed bound must include dock rotation baseline");
    requireNear(result.terminal.relativeAngularSpeedRadPerSec, 0.0, 1.0e-9,
                "capture must end co-rotating with the dock");
    requireNear(result.terminal.relativeLinearSpeedMetersPerSec, 0.0, 1.0e-8,
                "capture must end co-moving with moving/rotating dock frame");
}

void testContinuousCorridorRejectsBetweenSampleExcursion()
{
    Evaluator::Query query = baseQuery();
    query.durationSeconds = 1.0;
    query.hull.halfExtentsBodyMeters = {1.0, 0.5, 1.0};
    // With p0=p1=0 and v0=v1=10 m/s, the exact cubic-Hermite lateral
    // excursion peaks at 0.9622504486 m, while the largest of the 33 sampled
    // offsets is only 0.9613037109 m. With a 1.0 m projected hull half-width,
    // 1.9618 m keeps every point sample barely inside but the true curve exits.
    query.corridor.halfWidthMeters = 1.9618;
    query.corridor.halfHeightMeters = 10.0;
    query.start.pose.centerMapMeters = {};
    query.end.pose.centerMapMeters = {};
    query.start.linearVelocityMapMetersPerSec = {10.0, 0.0, 0.0};
    query.end.linearVelocityMapMetersPerSec = {10.0, 0.0, 0.0};

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::CorridorBlocked,
            "continuous dock-local corridor proof must reject hidden between-sample excursion");
    require(result.minimumSampleCorridorClearanceMeters > 0.0,
            "pinned fixture requires all 33 point samples to fit");
    require(result.minimumContinuousCorridorClearanceMeters < 0.0,
            "continuous corridor bound must be stricter than point samples");
}

void testLinearAuthorityRemainsInertialTruth()
{
    Evaluator::Query query = baseQuery();
    query.linearCapability.maxVerticalAccelerationMetersPerSec2 = 0.1;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::LinearAuthorityExceeded,
            "dock-local convenience must not invent world-space thrust authority");
    require(result.requiredPeakVerticalAccelerationMetersPerSec2 > 0.1,
            "linear authority result must expose required body-axis acceleration");
}

void testDockRotationConsumesAngularAuthority()
{
    Evaluator::Query query = baseQuery();
    query.durationSeconds = 1.0;
    const double omega = 0.5 * kPi;
    query.dock.angularVelocityMapRadPerSec = {0.0, 0.0, omega};
    query.start.linearVelocityMapMetersPerSec = {5.0 * omega, 5.0, 0.0};
    query.end.pose.bodyToMap = bottomToBottomShipBasisRotatedPlus90Z();
    query.angularCapability.maxAngularSpeedRadPerSec = 1.0;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::AngularAuthorityExceeded,
            "co-rotation baseline above ship angular-rate capability must fail");
    require(result.requiredPeakWorldAngularSpeedBoundRadPerSec > 1.0,
            "angular authority result must include dock angular speed");
}

void testTerminalRollMismatchSurvivesWideCorridor()
{
    Evaluator::Query query = baseQuery();
    query.start.pose.bodyToMap = bottomFaceButRolled180();
    query.end.pose.bodyToMap = bottomFaceButRolled180();
    query.corridor.halfWidthMeters = 10.0;
    query.corridor.halfHeightMeters = 10.0;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::TerminalNotCapturable,
            "wide corridor must not bypass terminal roll semantics");
    require(result.terminal.status == Terminal::Status::RollAlignmentMismatch,
            "180-degree rolled terminal state must be rejected by accepted 9A contract");
}

void testTerminalPositionMismatchIsNotCorridorSuccess()
{
    Evaluator::Query query = baseQuery();
    query.end.pose.centerMapMeters = {0.1, 0.0, 0.0};
    query.corridor.halfWidthMeters = 10.0;
    query.corridor.halfHeightMeters = 10.0;

    const Evaluator::Result result = Evaluator::evaluate(query);
    require(result.status == Evaluator::Status::TerminalNotCapturable,
            "corridor-feasible approach must still fail when terminal position is outside tolerance");
    require(result.terminal.status == Terminal::Status::PositionMismatch,
            "terminal mismatch must preserve the precise 9A failure reason");
}

void testNewtonianAndAssistedDockingSlipRemainDistinct()
{
    Evaluator::Query newton = baseQuery();
    newton.controlMode = Evaluator::ControlMode::Newtonian;
    const Evaluator::Result newtonResult = Evaluator::evaluate(newton);
    require(newtonResult.status == Evaluator::Status::FeasibleForCapture,
            "Newtonian docking approach may translate independently of nose direction");

    Evaluator::Query assisted = baseQuery();
    assisted.controlMode = Evaluator::ControlMode::EliteAssisted;
    assisted.assistedMaxVelocityToForwardAngleRad = 0.25;
    const Evaluator::Result assistedResult = Evaluator::evaluate(assisted);
    require(assistedResult.status == Evaluator::Status::AssistedSlipExceeded,
            "tight assisted policy must reject sideways docking translation");
}

} // namespace

int main()
{
    try
    {
        testStationaryDockFinalApproachIsContinuousAndCapturable();
        testTranslatingDockIsSolvedInRelativeFrame();
        testMovingRotatingDockEndsCoRotatingAtCapture();
        testContinuousCorridorRejectsBetweenSampleExcursion();
        testLinearAuthorityRemainsInertialTruth();
        testDockRotationConsumesAngularAuthority();
        testTerminalRollMismatchSurvivesWideCorridor();
        testTerminalPositionMismatchIsNotCorridorSuccess();
        testNewtonianAndAssistedDockingSlipRemainDistinct();

        std::cout << "NAVIGATION TRAJECTORY DOCKING APPROACH TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY DOCKING APPROACH TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
