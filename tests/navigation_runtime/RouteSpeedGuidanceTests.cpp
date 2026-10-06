#include "src/game/navigation/autopilot/RouteSpeedGuidance.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
using Guidance =
    game::navigation::autopilot::RouteSpeedGuidance;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testNoSlowdownWhenBelowCeiling()
{
    Guidance::TurnSlowdownRequest request;
    request.actualSpeedMps = 100.0;
    request.turnSpeedCeilingMps = 120.0;
    request.distanceToTurnMeters = 100.0;
    request.targetSetpointRateMps2 = 20.0;
    request.effectiveBrakingMps2 = 40.0;
    request.feedbackResponseSeconds = 0.2;

    const auto out = Guidance::evaluateTurnSlowdown(request);
    require(out.valid, "below-ceiling slowdown invalid");
    require(!out.slowdownRequiredNow,
        "below-ceiling speed incorrectly requested slowdown");
    require(out.speedDeltaMps == 0.0,
        "below-ceiling speed produced positive delta");
}

void testPreparationIncludesSlewBrakeAndFeedback()
{
    Guidance::TurnSlowdownRequest request;
    request.actualSpeedMps = 200.0;
    request.turnSpeedCeilingMps = 100.0;
    request.distanceToTurnMeters = 10000.0;
    request.targetSetpointRateMps2 = 20.0;
    request.effectiveBrakingMps2 = 50.0;
    request.feedbackResponseSeconds = 0.5;

    const auto out = Guidance::evaluateTurnSlowdown(request);
    require(out.valid, "preparation calculation invalid");

    // slew: 5s * 200 = 1000m
    // braking: (200^2 - 100^2)/(2*50) = 300m
    // feedback: 200*0.5 = 100m
    require(std::abs(out.setpointSlewSeconds - 5.0) < 1.0e-9,
        "setpoint slew wrong");
    require(std::abs(out.idealBrakeDistanceMeters - 300.0) < 1.0e-9,
        "brake distance wrong");
    require(std::abs(out.requiredPreparationDistanceMeters - 1400.0) < 1.0e-9,
        "combined preparation distance wrong");
    require(!out.slowdownRequiredNow,
        "far turn incorrectly requested immediate slowdown");
}

void testSlowdownStartsWhenPreparationTouchesTurn()
{
    Guidance::TurnSlowdownRequest request;
    request.actualSpeedMps = 200.0;
    request.turnSpeedCeilingMps = 100.0;
    request.distanceToTurnMeters = 1399.0;
    request.targetSetpointRateMps2 = 20.0;
    request.effectiveBrakingMps2 = 50.0;
    request.feedbackResponseSeconds = 0.5;

    const auto out = Guidance::evaluateTurnSlowdown(request);
    require(out.valid, "near-turn slowdown invalid");
    require(out.slowdownRequiredNow,
        "slowdown did not begin at preparation boundary");
}

}

int main()
{
    try
    {
        testNoSlowdownWhenBelowCeiling();
        testPreparationIncludesSlewBrakeAndFeedback();
        testSlowdownStartsWhenPreparationTouchesTurn();

        std::cout
            << "ROUTE SPEED GUIDANCE TESTS: PASS\n"
            << " - speed layer is scalar only\n"
            << " - slowdown distance includes slew, braking and feedback\n"
            << " - turn preparation starts before the authored restriction\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "ROUTE SPEED GUIDANCE TESTS: FAIL: "
            << e.what() << '\n';
        return 1;
    }
}
