#include "src/game/ship/control/ShipControlExecutionGateway.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

using Gateway =
    game::ship::control::ShipControlExecutionGateway;
using Path = Gateway::TranslationPath;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testManualInputOwnsTranslation()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;
    control.navigationPrecisionTranslationOnly = true;
    control.targetSpeedRate = 1.0f;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::ManualFlight,
        "manual longitudinal input did not override navigation translation"
    );
}

void testMainGimbalOwnsOrdinaryPhysicalTranslationPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;
    control.mainGimbalRightInput = 0.5f;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Newtonian
        ) == Path::ManualFlight,
        "Newtonian TVC command was bypassed by legacy navigation translation"
    );
}

void testPrecisionPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;
    control.navigationPrecisionTranslationOnly = true;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::NavigationPrecision,
        "precision navigation did not select precision translation path"
    );
}

void testAssistedPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::NavigationAssisted,
        "Assisted navigation did not select Assisted translation path"
    );
}

void testNewtonianNavigationDemandPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Newtonian
        ) == Path::NavigationAccelerationDemand,
        "Newtonian navigation did not select acceleration-demand path"
    );
}

void testNeutralManualPath()
{
    ShipControlState control;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::ManualFlight,
        "neutral ordinary control did not remain on manual flight path"
    );
}

} // namespace

int main()
{
    try
    {
        testManualInputOwnsTranslation();
        testMainGimbalOwnsOrdinaryPhysicalTranslationPath();
        testPrecisionPath();
        testAssistedPath();
        testNewtonianNavigationDemandPath();
        testNeutralManualPath();

        std::cout << "SHIP CONTROL EXECUTION GATEWAY TESTS: PASS\n";
        std::cout << " - manual/TVC input retains ordinary physical-path priority\n";
        std::cout << " - precision/Assisted/Newtonian execution selection is centralized\n";
        std::cout << " - callers no longer need to interpret ShipControlState\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "SHIP CONTROL EXECUTION GATEWAY TESTS: FAIL: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
