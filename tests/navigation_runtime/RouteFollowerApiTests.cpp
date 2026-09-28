#include "src/game/navigation/autopilot/RouteFollowerApi.h"

#include <iostream>
#include <stdexcept>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

} // namespace

int main()
{
    try
    {
        using Program = game::navigation::AcceptedManeuverProgram;

        Program program;
        program.valid = true;
        program.revision = 1;
        program.family = Program::ManeuverFamily::FreeTransit;
        program.referenceMode = Program::ReferenceMode::SpatialCorridor;
        program.translationMode = Program::TranslationMode::AssistedVelocity;
        program.acceptedAtUniverseTimeSeconds = 10.0;
        program.validUntilUniverseTimeSeconds = 30.0;
        program.sampleCount = 2;

        program.samples[0].timeOffsetSeconds = 0.0;
        program.samples[0].positionMapMeters = {0.0, 0.0, 0.0};
        program.samples[0].velocityMapMetersPerSecond = {5.0, 0.0, 0.0};
        program.samples[0].forwardMap = {1.0, 0.0, 0.0};
        program.samples[0].rightMap = {0.0, 0.0, 1.0};
        program.samples[0].upMap = {0.0, 1.0, 0.0};

        program.samples[1] = program.samples[0];
        program.samples[1].timeOffsetSeconds = 2.0;
        program.samples[1].positionMapMeters = {10.0, 0.0, 0.0};

        program.terminalTolerance.positionMeters = 0.5;
        program.terminalTolerance.linearVelocityMps = 1.0;
        program.terminalTolerance.forwardAngleRad = 0.2;
        program.terminalTolerance.angularVelocityRadPerSec = 0.2;

        program.tracking.positionErrorMeters = 20.0;
        program.tracking.linearVelocityErrorMps = 10.0;
        program.tracking.forwardAngleErrorRad = 0.5;
        program.tracking.angularVelocityErrorRadPerSec = 1.0;
        program.tracking.alongTrackPositionDeadbandMeters = 5.0;
        program.tracking.alongTrackSpeedDeadbandMps = 5.0;
        program.tracking.linearFeedbackReserveMps2 = 5.0;
        program.tracking.angularFeedbackReserveRadPerSec2 = 2.0;

        program.capability.maxForwardAccelerationMetersPerSec2 = 20.0;
        program.capability.maxReverseAccelerationMetersPerSec2 = 20.0;
        program.capability.maxLateralAccelerationMetersPerSec2 = 20.0;
        program.capability.maxVerticalAccelerationMetersPerSec2 = 20.0;
        program.capability.maxForwardMainAccelerationMetersPerSec2 = 20.0;
        program.capability.maxReverseMainAccelerationMetersPerSec2 = 20.0;
        program.capability.maxAngularAccelerationRadPerSec2 = 5.0;
        program.capability.maxAngularSpeedRadPerSec = 2.0;

        game::navigation::autopilot::RouteFollowerAgentState agent;
        agent.positionMapMeters = {1.0, 1.0, 0.0};
        agent.velocityMapMetersPerSecond = {5.0, 0.0, 0.0};
        agent.forwardMap = {1.0, 0.0, 0.0};
        agent.rightMap = {0.0, 0.0, 1.0};
        agent.upMap = {0.0, 1.0, 0.0};

        std::vector<Program> pages {program};
        const auto selection =
            game::navigation::autopilot::RouteFollower::selectPage(
                pages,
                10.2,
                agent.positionMapMeters,
                0
            );
        require(
            selection.status ==
                game::navigation::autopilot::
                    RouteProgramSelectionStatus::Active,
            "RouteFollower public API failed to select an active program page"
        );

        const auto sampled =
            game::navigation::autopilot::RouteFollower::sampleReference(
                program,
                10.2,
                agent.positionMapMeters,
                0
            );
        require(sampled.valid, "RouteFollower public API failed reference sampling");

        const auto alignment =
            game::navigation::autopilot::RouteFollower::alignToAttitude(
                program,
                agent,
                glm::dvec3(1.0, 0.0, 0.0),
                glm::dvec3(0.0, 0.0, 1.0),
                glm::dvec3(0.0, 1.0, 0.0),
                game::navigation::autopilot::RouteFollowerPolicy {}
            );
        require(alignment.valid, "RouteFollower public alignment API rejected valid state");

        const auto result =
            game::navigation::autopilot::RouteFollower::follow(
                program,
                10.2,
                agent,
                game::navigation::autopilot::RouteFollowerPolicy {},
                0
            );

        require(
            result.status !=
                game::navigation::autopilot::RouteFollowerStatus::InvalidInput,
            "RouteFollower public API rejected a valid spatial program"
        );
        require(result.spatialReference, "RouteFollower API lost spatial mode");

        std::cout << "ROUTE FOLLOWER API TESTS: PASS\n";
        std::cout << " - generic autopilot API executes without private tracker types\n";
        std::cout << " - page selection and reference sampling stay behind the same API\n";
        std::cout << " - attitude acquisition stays behind the same API\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ROUTE FOLLOWER API TESTS: FAIL: "
                  << error.what() << "\n";
        return 1;
    }
}
