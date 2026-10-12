#include "src/game/navigation/ManeuverCapabilityAdapters.h"

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
    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 50.0f;
    p.reverseMainEngineAccelerationMps2 = 50.0f;
    p.strafeAccel = 40.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.maxGs = 100.0f;
    p.maxLinearGs = 100.0f;
    return p;
}

void testNewtonianCourseAuthorityComesFromMainVector()
{
    auto p = params();

    const auto a =
        game::navigation::makeManeuverExecutionAuthority(
            p,
            game::navigation::LocalFlightControlLaw::Newtonian
        );

    requireNear(
        a.feedbackReserveMps2,
        10.0,
        1.0e-9,
        "Newtonian feedback reserve was still limited by 2 m/s^2 RCS"
    );
    requireNear(
        a.forwardAccelerationMps2,
        36.0,
        1.0e-9,
        "Newtonian forward planning authority disagrees with aft main"
    );
    requireNear(
        a.brakingAccelerationMps2,
        36.0,
        1.0e-9,
        "Newtonian flip-and-burn planning authority disagrees with aft main"
    );
    requireNear(
        a.lateralAccelerationMps2,
        36.0,
        1.0e-9,
        "Newtonian path-normal planning authority was still treated as RCS"
    );

    p.manoeuvreThrusterAccel = 100.0f;
    const auto withHugeRcs =
        game::navigation::makeManeuverExecutionAuthority(
            p,
            game::navigation::LocalFlightControlLaw::Newtonian
        );

    requireNear(
        withHugeRcs.lateralAccelerationMps2,
        a.lateralAccelerationMps2,
        1.0e-9,
        "Newtonian route authority changed when only precision RCS changed"
    );
    requireNear(
        withHugeRcs.feedbackReserveMps2,
        a.feedbackReserveMps2,
        1.0e-9,
        "Newtonian feedback reserve leaked precision RCS authority"
    );
}

void testAssistedAuthorityIsUnchanged()
{
    const auto p = params();

    const auto a =
        game::navigation::makeManeuverExecutionAuthority(
            p,
            game::navigation::LocalFlightControlLaw::Assisted
        );

    // Existing Assisted doctrine: reserve is 20% of weakest of
    // forward-main, reverse-main and automatic lateral stabilization.
    requireNear(a.feedbackReserveMps2, 8.0, 1.0e-9,
        "Assisted feedback reserve changed");
    requireNear(a.forwardAccelerationMps2, 37.8, 1.0e-9,
        "Assisted forward authority changed");
    requireNear(a.brakingAccelerationMps2, 37.8, 1.0e-9,
        "Assisted braking authority changed");
    requireNear(a.lateralAccelerationMps2, 28.8, 1.0e-9,
        "Assisted lateral authority changed");
}

void testNoAftMainMeansNoNewtonianRouteAuthority()
{
    auto p = params();
    p.forwardMainEngineAvailable = false;
    p.forwardMainEngineAccelerationMps2 = 0.0f;

    const auto a =
        game::navigation::makeManeuverExecutionAuthority(
            p,
            game::navigation::LocalFlightControlLaw::Newtonian
        );

    requireNear(a.feedbackReserveMps2, 0.0, 1.0e-12,
        "dead Newtonian main retained feedback reserve");
    requireNear(a.forwardAccelerationMps2, 0.0, 1.0e-12,
        "dead Newtonian main invented forward route authority");
    requireNear(a.brakingAccelerationMps2, 0.0, 1.0e-12,
        "dead Newtonian main invented flip-and-burn authority");
    requireNear(a.lateralAccelerationMps2, 0.0, 1.0e-12,
        "dead Newtonian main invented path-normal route authority");
}

} // namespace

int main()
{
    try
    {
        testNewtonianCourseAuthorityComesFromMainVector();
        testAssistedAuthorityIsUnchanged();
        testNoAftMainMeansNoNewtonianRouteAuthority();

        std::cout
            << "NEWTONIAN EXECUTION AUTHORITY: PASS\n"
            << " - route-vector authority comes from aft main\n"
            << " - precision RCS cannot inflate or starve route tracking\n"
            << " - Assisted authority remains unchanged\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NEWTONIAN EXECUTION AUTHORITY: FAIL: "
            << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
