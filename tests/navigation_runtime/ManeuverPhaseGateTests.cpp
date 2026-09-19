#include "src/game/navigation/ManeuverPhaseGate.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Gate = game::navigation::ManeuverPhaseGate;
using Program = game::navigation::AcceptedManeuverProgram;
using Follower = game::navigation::TrajectoryFollower;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Program programAt(double acceptedAt, double duration)
{
    Program p;
    p.valid = true;
    p.revision = 1;
    p.objectiveRevision = 1;
    p.acceptedAtUniverseTimeSeconds = acceptedAt;
    p.validUntilUniverseTimeSeconds = acceptedAt + duration + 10.0;
    p.sampleCount = 2;
    p.samples[0].timeOffsetSeconds = 0.0;
    p.samples[1].timeOffsetSeconds = duration;
    return p;
}

void testScheduledMovingAdvancesAtNominalEnd()
{
    const Program p = programAt(10.0, 2.0);

    Gate::Policy policy;
    policy.mode = Gate::Mode::ScheduledMoving;

    const auto before =
        Gate::evaluate(
            p,
            11.99,
            Follower::Status::Following,
            policy
        );
    require(before.status == Gate::Status::Continue,
            "ScheduledMoving advanced before nominal end");

    const auto atEnd =
        Gate::evaluate(
            p,
            12.0,
            Follower::Status::Following,
            policy
        );
    require(atEnd.status == Gate::Status::Advance,
            "ScheduledMoving did not advance at nominal end");
}

void testStateCaptureHoldsPastNominalEnd()
{
    const Program p = programAt(20.0, 3.0);

    Gate::Policy policy;
    policy.mode = Gate::Mode::StateCapture;
    policy.maximumCaptureOverrunSeconds = 4.0;

    const auto atEnd =
        Gate::evaluate(
            p,
            23.0,
            Follower::Status::Following,
            policy
        );
    require(atEnd.status == Gate::Status::Continue,
            "StateCapture advanced without terminal capture");

    const auto overrun =
        Gate::evaluate(
            p,
            25.5,
            Follower::Status::Following,
            policy
        );
    require(overrun.status == Gate::Status::Continue,
            "StateCapture timed out before bounded overrun elapsed");
    require(std::abs(overrun.captureOverrunSeconds - 2.5) < 1.0e-12,
            "StateCapture reported wrong overrun");
}

void testStateCaptureAdvancesOnlyOnFollowerComplete()
{
    const Program p = programAt(30.0, 1.0);

    Gate::Policy policy;
    policy.mode = Gate::Mode::StateCapture;
    policy.maximumCaptureOverrunSeconds = 5.0;

    const auto result =
        Gate::evaluate(
            p,
            33.0,
            Follower::Status::Complete,
            policy
        );

    require(result.status == Gate::Status::Advance,
            "StateCapture did not advance on real terminal capture");
    require(result.nominalEndReached,
            "StateCapture lost nominal-end witness");
    require(std::abs(result.captureOverrunSeconds - 2.0) < 1.0e-12,
            "StateCapture wrong capture-overrun witness");
}

void testStateCaptureTimesOutInsteadOfSilentlyAdvancing()
{
    const Program p = programAt(40.0, 2.0);

    Gate::Policy policy;
    policy.mode = Gate::Mode::StateCapture;
    policy.maximumCaptureOverrunSeconds = 3.0;

    const auto boundary =
        Gate::evaluate(
            p,
            45.0,
            Follower::Status::Following,
            policy
        );
    require(boundary.status == Gate::Status::Continue,
            "StateCapture timeout must be strictly after allowed overrun");

    const auto timedOut =
        Gate::evaluate(
            p,
            45.01,
            Follower::Status::Following,
            policy
        );
    require(timedOut.status == Gate::Status::CaptureTimedOut,
            "failed StateCapture silently advanced instead of timing out");
}

void testInvalidFollowerStateFailsClosed()
{
    const Program p = programAt(50.0, 1.0);
    Gate::Policy policy;

    const auto result =
        Gate::evaluate(
            p,
            51.0,
            Follower::Status::InvalidInput,
            policy
        );
    require(result.status == Gate::Status::InvalidInput,
            "phase gate accepted invalid follower state");
}

} // namespace

int main()
{
    try
    {
        testScheduledMovingAdvancesAtNominalEnd();
        testStateCaptureHoldsPastNominalEnd();
        testStateCaptureAdvancesOnlyOnFollowerComplete();
        testStateCaptureTimesOutInsteadOfSilentlyAdvancing();
        testInvalidFollowerStateFailsClosed();

        std::cout << "MANEUVER PHASE GATE TESTS: PASS\n";
        std::cout << " - ScheduledMoving advances at nominal reference end\n";
        std::cout << " - StateCapture holds terminal sample until real Follower::Complete\n";
        std::cout << " - failed capture times out explicitly instead of silently advancing\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER PHASE GATE TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
