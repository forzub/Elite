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

void testSteadyCruiseDoesNotChaseHullPose()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();
    request.actualVelocityMapMps = {80.0, 0.0, 0.0};
    request.desiredVelocityMapMps = request.actualVelocityMapMps;
    fillStraightPreview(request, 80.0);

    const auto control = controller.update(request, params());

    require(
        std::abs(control.targetSpeedRate) < 1.0e-5f,
        "steady Newtonian cruise invented a main-engine burn"
    );
    require(
        std::abs(control.pitchInput) +
        std::abs(control.yawInput) +
        std::abs(control.rollInput) < 1.0e-4f,
        "steady Newtonian cruise chased an unnecessary hull pose"
    );
    require(
        std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput) < 1.0e-6f,
        "steady Newtonian cruise leaked into translation RCS"
    );
}

void testFutureSlowdownStartsTurnBeforeBrakeStation()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();

    request.actualVelocityMapMps = {200.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {200.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {0.0, 0.0, 0.0};

    request.previewCount = 4;
    request.preview[0].timeOffsetSeconds = 0.75;
    request.preview[0].desiredVelocityMapMps = {200.0, 0.0, 0.0};
    request.preview[1].timeOffsetSeconds = 1.50;
    request.preview[1].desiredVelocityMapMps = {60.0, 0.0, 0.0};
    request.preview[2].timeOffsetSeconds = 2.25;
    request.preview[2].desiredVelocityMapMps = {40.0, 0.0, 0.0};
    request.preview[3].timeOffsetSeconds = 3.00;
    request.preview[3].desiredVelocityMapMps = {40.0, 0.0, 0.0};

    const auto control = controller.update(request, params());

    require(
        std::abs(control.targetSpeedRate) < 0.10f,
        "Newtonian optimizer accelerated into a known near-future slowdown"
    );
    require(
        std::abs(control.pitchInput) + std::abs(control.yawInput) > 1.0e-5f,
        "Newtonian optimizer did not pre-orient for near-future braking"
    );
    require(
        std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput) < 1.0e-6f,
        "pre-brake orientation spent translation RCS"
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

    const double baselineBurn = baselineControl.targetSpeedRate;
    const double shortBurn = shortControl.targetSpeedRate;
    const double shortTurn =
        std::abs(shortControl.pitchInput) +
        std::abs(shortControl.yawInput);

    require(
        shortBurn + 1.0e-4 < baselineBurn || shortTurn > 1.0e-4,
        "future slowdown did not change the short-straight optimum"
    );
}

void testGentleFutureTurnDoesNotUseTranslationRcs()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();

    request.actualVelocityMapMps = {80.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {80.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {0.0, 0.0, 4.0};

    constexpr double Deg = 3.1415926535897932384626433832795 / 180.0;
    request.previewCount = 4;
    for (std::size_t i = 0; i < request.previewCount; ++i)
    {
        const double angle = (2.0 + 3.0 * static_cast<double>(i)) * Deg;
        request.preview[i].timeOffsetSeconds =
            0.75 * static_cast<double>(i + 1);
        request.preview[i].desiredVelocityMapMps = {
            80.0 * std::cos(angle),
            0.0,
            80.0 * std::sin(angle)
        };
        request.preview[i].desiredLinearAccelerationMapMps2 =
            {0.0, 0.0, 4.0};
    }

    const auto control = controller.update(request, params());

    require(
        std::abs(control.mainGimbalRightInput) +
        std::abs(control.pitchInput) +
        std::abs(control.yawInput) > 1.0e-5f,
        "gentle future turn produced no Newtonian thrust-vector response"
    );
    require(
        std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput) < 1.0e-6f,
        "gentle turn substituted translation RCS for main-vector control"
    );
}

void testCrossTrackCorrectionUsesMainVector()
{
    NewtonianOptimalController controller;
    auto request = baseRequest();

    request.positionReferenceValid = true;
    request.actualPositionMapMeters = {0.0, 20.0, 0.0};
    request.desiredPositionMapMeters = {0.0, 0.0, 0.0};
    request.actualVelocityMapMps = {80.0, 0.0, 0.0};
    request.desiredVelocityMapMps = {80.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {0.0, -10.0, 0.0};

    fillStraightPreview(request, 80.0);
    for (std::size_t i = 0; i < request.previewCount; ++i)
        request.preview[i].desiredPositionMapMeters =
            {80.0 * request.preview[i].timeOffsetSeconds, 0.0, 0.0};

    const auto control = controller.update(request, params());

    require(
        std::abs(control.mainGimbalUpInput) +
        std::abs(control.pitchInput) +
        std::abs(control.yawInput) > 1.0e-5f,
        "cross-track error produced no main-vector correction"
    );
    require(
        std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput) < 1.0e-6f,
        "cross-track recovery leaked into translation RCS"
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
        std::abs(control.forwardInput) +
        std::abs(control.strafeInput) +
        std::abs(control.liftInput) > 1.0e-6f,
        "Newtonian precision stop lost its RCS trim"
    );
}

} // namespace

int main()
{
    try
    {
        testSteadyCruiseDoesNotChaseHullPose();
        testFutureSlowdownStartsTurnBeforeBrakeStation();
        testPreviewChangesShortStraightDecision();
        testGentleFutureTurnDoesNotUseTranslationRcs();
        testCrossTrackCorrectionUsesMainVector();
        testPrecisionStopStillUsesRcs();

        std::cout
            << "NEWTONIAN OPTIMAL CONTROLLER: PASS\n"
            << " - steady cruise coasts without hull chasing\n"
            << " - future braking is anticipated before the brake station\n"
            << " - short-straight decisions depend on future constraints\n"
            << " - gentle turns use main-vector control, not translation RCS\n"
            << " - cross-track recovery is priced and remains main-vector owned\n"
            << " - precision-stop RCS remains isolated\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NEWTONIAN OPTIMAL CONTROLLER: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
