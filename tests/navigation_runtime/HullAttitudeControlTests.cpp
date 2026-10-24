#include "src/game/navigation/autopilot/HullAttitudeControl.h"

#include <algorithm>
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
    r.pitchYawAngularAccelerationAuthorityRadPerSec2 = 3.0;
    r.rollAngularAccelerationAuthorityRadPerSec2 = 3.0;
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

void testDamagedIndependentAttitudeChannels()
{
    auto r=base();
    r.pitchYawErrorRad={0.0,0.3};
    r.rollErrorRad=0.5;
    r.rollAngularAccelerationAuthorityRadPerSec2=0.0;
    const auto noRoll=Control::evaluate(r);
    require(noRoll.valid &&
            noRoll.pitchYawInput.y>0.01 &&
            std::abs(noRoll.rollInput)<1.0e-12,
            "lost roll authority incorrectly disabled live yaw steering");

    r.rollAngularAccelerationAuthorityRadPerSec2=3.0;
    r.pitchYawAngularAccelerationAuthorityRadPerSec2=0.0;
    const auto noPlanar=Control::evaluate(r);
    require(noPlanar.valid &&
            std::abs(noPlanar.rollInput)>0.01 &&
            glm::length(noPlanar.pitchYawInput)<1.0e-12,
            "lost pitch/yaw authority incorrectly disabled live roll");

    r.rollAngularAccelerationAuthorityRadPerSec2=0.0;
    require(!Control::evaluate(r).valid,
            "attitude control invented authority with no available axes");
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


void testMovingCourseRateIsTrackedWithoutWaitingForAngleError()
{
    auto r = base();
    r.desiredPitchYawRateRadPerSec = {0.0, 0.25};

    const auto accelerate = Control::evaluate(r);
    require(accelerate.valid, "moving-course control invalid");
    require(
        accelerate.pitchYawInput.y > 0.1,
        "moving course did not command yaw before angle error accumulated"
    );

    r.pitchYawRateRadPerSec = {0.0, 0.25};
    const auto matched = Control::evaluate(r);
    require(matched.valid, "matched moving-course control invalid");
    require(
        glm::length(matched.pitchYawInput) < 1.0e-9,
        "matched course rate still produced a corrective kick"
    );
}


void testDesiredRollRateTracksFrameWithoutWaitingForAngleKick()
{
    auto r = base();
    r.desiredRollRateRadPerSec = 0.30;

    const auto accelerate = Control::evaluate(r);
    require(accelerate.valid, "desired-roll-rate control invalid");
    require(
        accelerate.rollInput > 0.1,
        "desired roll rate did not command frame-alignment roll"
    );

    r.rollRateRadPerSec = 0.30;
    const auto matched = Control::evaluate(r);
    require(matched.valid, "matched desired-roll-rate control invalid");
    require(
        std::abs(matched.rollInput) < 1.0e-9,
        "matched desired roll rate still produced an impulse"
    );
}

void testAssistedRollFollowsTunnelRateWithoutBangBang()
{
    auto r=base();
    r.rollResponseSeconds=0.50;
    r.rollErrorRad=0.08;
    // HullTunnelRollGuidance already turns pose error into a desired
    // angular rate using its finite ~1.2 s physical roll response.
    r.desiredRollRateRadPerSec=0.08/1.2;

    const auto initial=Control::evaluate(r);
    require(initial.valid &&
            initial.rollInput>0.001 &&
            initial.rollInput<0.25,
            "healthy Assisted used full roll torque for a tiny tunnel correction");

    r.rollRateRadPerSec=r.desiredRollRateRadPerSec;
    const auto matched=Control::evaluate(r);
    require(matched.valid && std::abs(matched.rollInput)<1.0e-8,
            "Assisted kept accelerating after reaching requested roll rate");

    r.rollErrorRad=0.0;
    r.desiredRollRateRadPerSec=0.0;
    const auto arrested=Control::evaluate(r);
    require(arrested.valid && arrested.rollInput<-0.01 &&
            arrested.rollInput>-0.25,
            "Assisted could not gently arrest the existing roll rate");
}

void testAssistedRollConvergesWithoutLimitCycle()
{
    auto r=base();
    r.rollResponseSeconds=0.50;
    r.rollAngularAccelerationAuthorityRadPerSec2=2.451663;
    r.pitchYawErrorRad={0.0,0.0};
    double rollError=0.16;
    double rollRate=0.0;
    double peakCommand=0.0;
    for(int i=0;i<500;++i)
    {
        r.rollErrorRad=rollError;
        r.rollRateRadPerSec=rollRate;
        r.desiredRollRateRadPerSec=rollError/1.22;
        const auto output=Control::evaluate(r);
        require(output.valid,"damped roll simulation produced invalid controls");
        require(glm::length(output.pitchYawInput)<1.0e-12,
                "roll stabilization demanded pitch/yaw");
        peakCommand=std::max(peakCommand,std::abs(output.rollInput));
        const double acceleration=output.rollInput*
            r.rollAngularAccelerationAuthorityRadPerSec2;
        rollRate+=acceleration*r.deltaSeconds;
        rollError-=rollRate*r.deltaSeconds;
    }
    require(peakCommand<0.5,
            "healthy small-angle Assisted roll saturated the torque command");
    require(std::abs(rollError)<0.002 && std::abs(rollRate)<0.002,
            "healthy Assisted roll did not settle on a constant tunnel phase");
}

void testAssistedRollTracksMovingTunnelWithoutRocking()
{
    auto r=base();
    r.rollResponseSeconds=0.50;
    r.rollAngularAccelerationAuthorityRadPerSec2=2.451663;
    constexpr double tunnelRollRate=0.08;
    double signedError=0.16;
    double hullRollRate=0.0;
    double peakCommand=0.0;

    // A station/tunnel can rotate continuously. The outer two-point
    // guidance returns tunnel phase rate PLUS pose-error correction.
    // A healthy craft must catch that frame without +/- full roll pulses.
    for(int tick=0;tick<500;++tick)
    {
        r.rollErrorRad=signedError;
        r.rollRateRadPerSec=hullRollRate;
        r.desiredRollRateRadPerSec=tunnelRollRate+signedError/1.22;
        const auto output=Control::evaluate(r);
        require(output.valid,"moving-tunnel roll command invalid");
        peakCommand=std::max(peakCommand,std::abs(output.rollInput));
        hullRollRate+=r.rollAngularAccelerationAuthorityRadPerSec2*
            output.rollInput*r.deltaSeconds;
        signedError+=(tunnelRollRate-hullRollRate)*r.deltaSeconds;
    }
    require(peakCommand<0.5,
            "healthy Assisted moving-tunnel roll demanded full power");
    require(std::abs(signedError)<0.002 &&
            std::abs(hullRollRate-tunnelRollRate)<0.002,
            "healthy Assisted did not match moving tunnel roll phase/rate");
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
        testDamagedIndependentAttitudeChannels();
        testResidualRollRateDoesNotCreatePitchYaw();
        testResidualYawRateDoesNotCreateRoll();
        testMovingCourseRateIsTrackedWithoutWaitingForAngleError();
        testDesiredRollRateTracksFrameWithoutWaitingForAngleKick();
        testAssistedRollFollowsTunnelRateWithoutBangBang();
        testAssistedRollConvergesWithoutLimitCycle();
        testAssistedRollTracksMovingTunnelWithoutRocking();

        std::cout
            << "HULL ATTITUDE CONTROL TESTS: PASS\n"
            << " - roll cannot consume pitch/yaw authority\n"
            << " - pitch/yaw cannot consume roll authority\n"
            << " - roll-rate damping cannot create pitch/yaw\n"
            << " - yaw/pitch damping cannot create roll\n"
            << " - moving velocity-course rate is tracked continuously\n"
            << " - dock-frame roll rate is tracked continuously\n";
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
