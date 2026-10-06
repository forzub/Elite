#include "src/game/navigation/autopilot/HullAttitudeControl.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
using Control =
    game::navigation::autopilot::HullAttitudeControl;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Control::Request base()
{
    Control::Request r;
    r.maxPitchYawRateRadPerSec = {2.5, 2.5};
    r.maxRollRateRadPerSec = 3.0;
    r.angularAccelerationAuthorityRadPerSec2 = 3.0;
    r.deltaSeconds = 0.02;
    return r;
}

void testRollDoesNotReduceYawPitchAuthority()
{
    auto noseOnly = base();
    noseOnly.pitchYawErrorRad = {0.0, 0.5};

    auto noseAndRoll = noseOnly;
    noseAndRoll.rollErrorRad = 1.5;

    const auto a = Control::evaluate(noseOnly);
    const auto b = Control::evaluate(noseAndRoll);

    require(a.valid && b.valid, "nose/roll control invalid");
    require(
        glm::length(a.pitchYawInput) > 0.9,
        "nose-only test did not demand strong pitch/yaw"
    );
    require(
        glm::length(b.pitchYawInput - a.pitchYawInput) < 1.0e-12,
        "roll demand reduced or changed pitch/yaw authority"
    );
    require(
        std::abs(b.rollInput) > 0.9,
        "combined request lost independent roll authority"
    );
}

void testYawPitchDoesNotReduceRollAuthority()
{
    auto rollOnly = base();
    rollOnly.rollErrorRad = 1.5;

    auto combined = rollOnly;
    combined.pitchYawErrorRad = {0.4, 0.4};

    const auto a = Control::evaluate(rollOnly);
    const auto b = Control::evaluate(combined);

    require(a.valid && b.valid, "roll/nose control invalid");
    require(
        std::abs(a.rollInput) > 0.9,
        "roll-only test did not demand strong roll"
    );
    require(
        std::abs(b.rollInput - a.rollInput) < 1.0e-12,
        "pitch/yaw demand reduced or changed roll authority"
    );
    require(
        glm::length(b.pitchYawInput) > 0.9,
        "combined request lost independent pitch/yaw authority"
    );
}

void testResidualRollRateDoesNotCreatePitchYaw()
{
    auto r = base();
    r.rollRateRadPerSec = 1.0;

    const auto out = Control::evaluate(r);
    require(out.valid, "residual-roll-rate control invalid");
    require(
        glm::length(out.pitchYawInput) < 1.0e-12,
        "roll-rate damping leaked into pitch/yaw"
    );
    require(
        out.rollInput < -0.1,
        "roll-rate damping did not oppose residual roll"
    );
}

void testResidualYawRateDoesNotCreateRoll()
{
    auto r = base();
    r.pitchYawRateRadPerSec = {0.0, 1.0};

    const auto out = Control::evaluate(r);
    require(out.valid, "residual-yaw-rate control invalid");
    require(
        std::abs(out.rollInput) < 1.0e-12,
        "yaw-rate damping leaked into roll"
    );
    require(
        out.pitchYawInput.y < -0.1,
        "yaw-rate damping did not oppose residual yaw"
    );
}

}

int main()
{
    try
    {
        testRollDoesNotReduceYawPitchAuthority();
        testYawPitchDoesNotReduceRollAuthority();
        testResidualRollRateDoesNotCreatePitchYaw();
        testResidualYawRateDoesNotCreateRoll();

        std::cout
            << "HULL ATTITUDE CONTROL TESTS: PASS\n"
            << " - roll cannot consume pitch/yaw authority\n"
            << " - pitch/yaw cannot consume roll authority\n"
            << " - roll-rate damping cannot create pitch/yaw\n"
            << " - yaw/pitch damping cannot create roll\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "HULL ATTITUDE CONTROL TESTS: FAIL: "
            << e.what() << '\n';
        return 1;
    }
}
