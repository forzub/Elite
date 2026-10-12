#include "src/game/navigation/DynamicMotionSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

ShipParams params()
{
    ShipParams p {};
    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;

    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = false;
    p.forwardMainEngineAccelerationMps2 = 10.0f;
    p.reverseMainEngineAccelerationMps2 = 0.0f;

    p.mainGimbalPitchLimitRad = 0.122173048f; // 7 deg
    p.mainGimbalYawLimitRad = 0.122173048f;   // 7 deg
    p.mainGimbalRateRadPerSec = 0.139626340f;// 8 deg/s

    p.manoeuvreThrusterAccel = 0.0f;
    p.strafeAccel = 0.0f;
    p.maxGs = 100.0f;
    p.maxLinearGs = 100.0f;
    p.turnRadius = 20.0f;
    return p;
}

game::navigation::KinematicFrame frame()
{
    game::navigation::KinematicFrame f;
    f.systemId = 0;
    f.frameId = "gimbal-test";
    f.valid = true;
    return f;
}

void apply(
    game::navigation::DynamicMotionState& motion,
    const ShipParams& vehicle,
    double dt
)
{
    const auto f = frame();
    game::navigation::DynamicMotionSystem::applyLocalFrameInput(
        motion,
        f,
        vehicle,
        static_cast<float>(dt),
        1.0f,
        false,
        0.0f,
        0.0f,
        0.0f,
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

double thrustYawRad(
    const game::navigation::DynamicMotionState& motion
)
{
    const glm::dvec3 a = motion.mainEngineAccelerationMps2;
    return std::atan2(a.z, a.x);
}

void testGimbalSlewIsRateLimited()
{
    auto vehicle = params();
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    motion.mainGimbalRightCommand01 = 1.0;

    apply(motion, vehicle, 0.25);

    constexpr double TwoDeg =
        2.0 * 3.1415926535897932384626433832795 / 180.0;

    requireNear(
        motion.mainGimbalYawRad,
        TwoDeg,
        2.0e-6,
        "main nozzle ignored 8 deg/s physical slew limit"
    );
    requireNear(
        thrustYawRad(motion),
        TwoDeg,
        2.0e-6,
        "main acceleration did not follow actual nozzle angle"
    );
    require(
        glm::length(motion.manoeuvreAccelerationMps2) < 1.0e-12,
        "TVC leaked into manoeuvre/RCS acceleration"
    );

    apply(motion, vehicle, 1.0);

    constexpr double SevenDeg =
        7.0 * 3.1415926535897932384626433832795 / 180.0;

    requireNear(
        motion.mainGimbalYawRad,
        SevenDeg,
        2.0e-6,
        "main nozzle exceeded or failed to reach 7 deg travel"
    );
    requireNear(
        thrustYawRad(motion),
        SevenDeg,
        2.0e-6,
        "main thrust vector disagrees with saturated nozzle state"
    );
}

void testDiagonalCommandCannotCheatEnvelope()
{
    auto vehicle = params();
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    motion.mainGimbalRightCommand01 = 1.0;
    motion.mainGimbalUpCommand01 = 1.0;

    apply(motion, vehicle, 0.25);

    constexpr double TwoDeg =
        2.0 * 3.1415926535897932384626433832795 / 180.0;
    const double firstSlew =
        std::sqrt(
            motion.mainGimbalYawRad * motion.mainGimbalYawRad +
            motion.mainGimbalPitchRad * motion.mainGimbalPitchRad
        );
    requireNear(
        firstSlew,
        TwoDeg,
        2.0e-6,
        "diagonal nozzle slew exceeded resultant 8 deg/s rate"
    );

    apply(motion, vehicle, 2.0);

    const glm::dvec3 a =
        glm::normalize(motion.mainEngineAccelerationMps2);
    const double resultant =
        std::acos(
            std::clamp(a.x, -1.0, 1.0)
        );

    constexpr double SevenDeg =
        7.0 * 3.1415926535897932384626433832795 / 180.0;

    require(
        resultant <= SevenDeg + 2.0e-4,
        "diagonal TVC command escaped the physical 7 deg envelope"
    );
    require(
        motion.mainGimbalYawRad > 0.0 &&
        motion.mainGimbalPitchRad > 0.0,
        "diagonal TVC command did not move both nozzle axes"
    );
}

} // namespace

int main()
{
    try
    {
        testGimbalSlewIsRateLimited();
        testDiagonalCommandCannotCheatEnvelope();

        std::cout
            << "NEWTONIAN MAIN GIMBAL: PASS\n"
            << " - nozzle travel is rate-limited\n"
            << " - main thrust follows actual nozzle direction\n"
            << " - diagonal command stays inside the physical envelope\n"
            << " - TVC does not become manoeuvre/RCS acceleration\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NEWTONIAN MAIN GIMBAL: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
