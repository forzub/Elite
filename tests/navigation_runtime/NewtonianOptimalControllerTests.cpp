#include "src/game/navigation/autopilot/NewtonianOptimalController.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using game::navigation::autopilot::AutopilotControlRequest;
using game::navigation::autopilot::NewtonianOptimalController;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipParams params()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;

    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;

    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = false;
    p.forwardMainEngineAccelerationMps2 = 50.0f;
    p.reverseMainEngineAccelerationMps2 = 0.0f;

    p.mainGimbalPitchLimitRad = 0.122173048f; // 7 deg
    p.mainGimbalYawLimitRad = 0.122173048f;   // 7 deg
    p.mainGimbalRateRadPerSec = 0.139626340f;// 8 deg/s

    p.manoeuvreThrusterAccel = 2.0f;
    p.strafeAccel = 2.0f;
    p.maxGs = 100.0f;
    p.maxLinearGs = 100.0f;
    return p;
}

AutopilotControlRequest baseRequest()
{
    AutopilotControlRequest r;
    r.forwardMap = {1.0, 0.0, 0.0};
    r.rightMap = {0.0, 0.0, 1.0};
    r.upMap = {0.0, 1.0, 0.0};
    r.desiredForwardMap = r.forwardMap;
    r.desiredUpMap = r.upMap;
    r.deltaSeconds = 0.02;
    return r;
}

void fillStraightPreview(
    AutopilotControlRequest& request,
    double speedMps
)
{
    request.previewCount = 4;
    for (std::size_t i = 0; i < request.previewCount; ++i)
    {
        auto& s = request.preview[i];
        s.timeOffsetSeconds = 0.75 * static_cast<double>(i + 1);
        s.desiredVelocityMapMps = {speedMps, 0.0, 0.0};
        s.desiredLinearAccelerationMapMps2 = {0.0, 0.0, 0.0};
    }
}

// Corridor reference on the +X axis with a 64 m tracking tolerance.
void onCorridor(
    AutopilotControlRequest& request,
    const glm::dvec3& offsetMeters
)
{
    request.positionReferenceValid = true;
    request.corridorToleranceMeters = 64.0;
    request.desiredPositionMapMeters = {0.0, 0.0, 0.0};
    request.actualPositionMapMeters = offsetMeters;
}

double rotationInput(const ShipControlState& control)
{
    return std::abs(control.pitchInput) + std::abs(control.yawInput);
}

double translationInput(const ShipControlState& control)
{
    return std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput);
}

void testZeroSpeedAccelerationReferenceLaunchesMain()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();
    onCorridor(request, {0.0, 0.0, 0.0});

    request.actualVelocityMapMps = {0.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {0.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {45.0, 0.0, 0.0};

    request.previewCount = 4;
    for (std::size_t i = 0; i < request.previewCount; ++i)
    {
        const double t = 0.75 * static_cast<double>(i + 1);
        auto& s = request.preview[i];
        s.timeOffsetSeconds = t;
        s.desiredPositionMapMeters = {0.5 * 45.0 * t * t, 0.0, 0.0};
        s.desiredVelocityMapMps = {45.0 * t, 0.0, 0.0};
        s.desiredLinearAccelerationMapMps2 = {45.0, 0.0, 0.0};
    }

    const auto control = controller.update(request, params());

    require(
        control.targetSpeedRate > 0.10f,
        "zero-speed Newtonian launch stalled despite positive acceleration reference"
    );
    require(
        translationInput(control) < 1.0e-6,
        "aligned launch burn leaked into translation RCS"
    );
    require(
        control.rcsKeepsTranslationDeltaV,
        "autopilot RCS delta-v is not marked as owned by the trajectory"
    );
}

void testSteadyCruiseCoasts()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();
    onCorridor(request, {0.0, 0.0, 0.0});
    request.actualVelocityMapMps = {80.0, 0.0, 0.0};
    request.desiredVelocityMapMps = request.actualVelocityMapMps;
    fillStraightPreview(request, 80.0);

    const auto control = controller.update(request, params());

    require(
        std::abs(control.targetSpeedRate) < 1.0e-5f,
        "steady Newtonian cruise invented a main-engine burn"
    );
    require(
        rotationInput(control) + std::abs(control.rollInput) < 1.0e-4,
        "steady Newtonian cruise chased an unnecessary hull pose"
    );
    require(
        translationInput(control) < 1.0e-6,
        "steady Newtonian cruise leaked into translation RCS"
    );
}

void testSmallOffsetOnSafeVelocityStaysBallistic()
{
    // Centreline is a reference, not a rail: 10 m off-centre on a velocity
    // that keeps the craft inside the tube needs no correction at all.
    NewtonianOptimalController controller;
    auto request = baseRequest();
    onCorridor(request, {0.0, 10.0, 0.0});
    request.actualVelocityMapMps = {200.0, 0.0, 0.0};
    request.desiredVelocityMapMps = request.actualVelocityMapMps;
    fillStraightPreview(request, 200.0);

    const auto control = controller.update(request, params());

    require(
        translationInput(control) < 1.0e-6 &&
        std::abs(control.targetSpeedRate) < 1.0e-5f &&
        rotationInput(control) < 1.0e-4,
        "a small offset on a safe velocity triggered a correction"
    );
}

void testPredictedWallExitIsTrimmedWithRcs()
{
    // On centre now, but 6 m/s lateral drift leaves the safe tube within the
    // prediction horizon: correct at once, with RCS and without the hull.
    NewtonianOptimalController controller;
    auto request = baseRequest();
    onCorridor(request, {0.0, 2.0, 0.0});
    request.actualVelocityMapMps = {200.0, 6.0, 0.0};
    request.desiredVelocityMapMps = {200.0, 0.0, 0.0};
    fillStraightPreview(request, 200.0);

    const auto control = controller.update(request, params());

    require(
        control.liftInput < -1.0e-3f,
        "predicted corridor exit was not trimmed with RCS toward the centre"
    );
    require(
        std::abs(control.targetSpeedRate) < 1.0e-5f &&
        rotationInput(control) < 1.0e-4,
        "a few m/s of lateral drift turned the hull or fired main"
    );
}

void testFutureSlowdownStartsCalmFlipBeforeBrakeStation()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();
    onCorridor(request, {0.0, 0.0, 0.0});

    request.actualVelocityMapMps = {200.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {200.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {0.0, 0.0, 0.0};

    request.previewCount = 4;
    request.preview[0].timeOffsetSeconds = 2.0;
    request.preview[0].desiredVelocityMapMps = {200.0, 0.0, 0.0};
    request.preview[1].timeOffsetSeconds = 4.0;
    request.preview[1].desiredVelocityMapMps = {200.0, 0.0, 0.0};
    request.preview[2].timeOffsetSeconds = 6.0;
    request.preview[2].desiredVelocityMapMps = {60.0, 0.0, 0.0};
    request.preview[3].timeOffsetSeconds = 8.0;
    request.preview[3].desiredVelocityMapMps = {40.0, 0.0, 0.0};

    const auto control = controller.update(request, params());

    require(
        std::abs(control.targetSpeedRate) < 1.0e-5f,
        "planned flip fired the main engine before the hull was turned"
    );
    require(
        rotationInput(control) > 1.0e-3,
        "braking ahead did not start the flip before the brake station"
    );
    require(
        glm::length(glm::dvec2(control.pitchInput, control.yawInput)) <= 0.51,
        "planned flip used more than the calm preferred angular share"
    );
    require(
        translationInput(control) < 1.0e-6,
        "planned flip spent translation RCS while coasting"
    );
}

void testBurnGateHoldsMainUntilThrustIsAligned()
{
    auto vehicle = params();
    constexpr double Deg = 3.1415926535897932384626433832795 / 180.0;
    const auto brakingAt =
        [&](double hullErrorDeg)
        {
            // Braking is due now (-X); the hull points hullErrorDeg away
            // from the retro direction.
            auto request = baseRequest();
            onCorridor(request, {0.0, 0.0, 0.0});
            const double a = (180.0 - hullErrorDeg) * Deg;
            request.forwardMap = {std::cos(a), 0.0, std::sin(a)};
            request.rightMap = {-std::sin(a), 0.0, std::cos(a)};
            request.actualVelocityMapMps = {150.0, 0.0, 0.0};
            request.desiredVelocityMapMps = {150.0, 0.0, 0.0};
            request.desiredLinearAccelerationMapMps2 = {-40.0, 0.0, 0.0};
            NewtonianOptimalController controller;
            return controller.update(request, vehicle);
        };

    require(
        brakingAt(60.0).targetSpeedRate < 1.0e-5f &&
        brakingAt(20.0).targetSpeedRate < 1.0e-5f,
        "strong main fired while the thrust vector was still turned away"
    );
    require(
        brakingAt(60.0).pitchInput != 0.0f ||
        brakingAt(60.0).yawInput != 0.0f,
        "gated burn did not keep turning the hull"
    );
    require(
        brakingAt(8.0).targetSpeedRate > 0.5f,
        "braking burn did not start once the nozzle cone covered the error"
    );
    require(
        std::abs(brakingAt(8.0).mainGimbalRightInput) > 0.5f,
        "aligned burn did not use nozzle vectoring for the residual error"
    );
}

void testPreviewChangesShortStraightDecision()
{
    auto vehicle = params();

    auto baselineRequest = baseRequest();
    baselineRequest.actualVelocityMapMps = {80.0, 0.0, 0.0};
    baselineRequest.desiredVelocityMapMps = {160.0, 0.0, 0.0};
    baselineRequest.desiredLinearAccelerationMapMps2 = {40.0, 0.0, 0.0};
    fillStraightPreview(baselineRequest, 160.0);

    NewtonianOptimalController baseline;
    const auto baselineControl =
        baseline.update(baselineRequest, vehicle);

    auto shortRequest = baselineRequest;
    shortRequest.preview[1].desiredVelocityMapMps = {70.0, 0.0, 0.0};
    shortRequest.preview[2].desiredVelocityMapMps = {50.0, 0.0, 0.0};
    shortRequest.preview[3].desiredVelocityMapMps = {50.0, 0.0, 0.0};

    NewtonianOptimalController shortController;
    const auto shortControl =
        shortController.update(shortRequest, vehicle);

    require(
        shortControl.targetSpeedRate + 1.0e-4 <
                baselineControl.targetSpeedRate ||
            rotationInput(shortControl) > 1.0e-4,
        "future slowdown did not change the short-straight decision"
    );
}

void testPrecisionStopStillUsesRcs()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();
    request.actualVelocityMapMps = {0.10, 0.0, 0.0};
    request.desiredVelocityMapMps = {0.0, 0.0, 0.0};
    request.stopRequested = true;

    const auto control = controller.update(request, params());

    require(
        translationInput(control) > 1.0e-6,
        "Newtonian precision stop lost its RCS trim"
    );
}

} // namespace

int main()
{
    try
    {
        testZeroSpeedAccelerationReferenceLaunchesMain();
        testSteadyCruiseCoasts();
        testSmallOffsetOnSafeVelocityStaysBallistic();
        testPredictedWallExitIsTrimmedWithRcs();
        testFutureSlowdownStartsCalmFlipBeforeBrakeStation();
        testBurnGateHoldsMainUntilThrustIsAligned();
        testPreviewChangesShortStraightDecision();
        testPrecisionStopStillUsesRcs();

        std::cout
            << "NEWTONIAN BALLISTIC CONTROLLER: PASS\n"
            << " - zero-speed positive-acceleration reference launches main\n"
            << " - steady cruise coasts without hull chasing\n"
            << " - a small offset on a safe velocity stays ballistic\n"
            << " - a predicted corridor exit is trimmed with RCS, not the hull\n"
            << " - braking ahead starts a calm flip before the brake station\n"
            << " - the burn gate holds main until the thrust is aligned\n"
            << " - short-straight decisions depend on future constraints\n"
            << " - precision-stop RCS remains isolated\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NEWTONIAN BALLISTIC CONTROLLER: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
