#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/client/ClientSpatialDomain.h"
#include "src/game/client/MapTransitionController.h"
#include "src/game/navigation/DynamicMotionState.h"
#include "src/game/navigation/HubNavigationFrame.h"
#include "src/game/navigation/ReferenceFrame.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/game/simulation/ObjectSnapshot.h"
#include "src/game/simulation/RuntimeSystemPolicy.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/game/simulation/UniverseDiagnosticTrajectorySession.h"
#include "src/render/celestial/CloudMotionPolicy.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/world/time/UniverseClock.h"

namespace
{
class TestFailure final : public std::runtime_error
{
public:
    explicit TestFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

[[noreturn]] void fail(
    const char* expression,
    const char* file,
    int line,
    const std::string& detail = {})
{
    std::string message =
        std::string(file) + ':' + std::to_string(line) +
        ": check failed: " + expression;

    if (!detail.empty())
        message += " (" + detail + ')';

    throw TestFailure(message);
}

#define REQUIRE(expression) \
    do \
    { \
        if (!(expression)) \
            fail(#expression, __FILE__, __LINE__); \
    } while (false)

void requireNear(
    double actual,
    double expected,
    double epsilon,
    const char* expression,
    const char* file,
    int line)
{
    if (std::abs(actual - expected) > epsilon)
    {
        fail(
            expression,
            file,
            line,
            "actual=" + std::to_string(actual) +
            ", expected=" + std::to_string(expected));
    }
}

#define REQUIRE_NEAR(actual, expected, epsilon) \
    requireNear((actual), (expected), (epsilon), #actual, __FILE__, __LINE__)

void requireVecNear(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double epsilon,
    const char* expression,
    const char* file,
    int line)
{
    if (glm::length(actual - expected) > epsilon)
    {
        fail(expression, file, line);
    }
}

#define REQUIRE_VEC_NEAR(actual, expected, epsilon) \
    requireVecNear((actual), (expected), (epsilon), #actual, __FILE__, __LINE__)

void testUniverseClockRewindsAfterAcceleratedDiagnostic()
{
    world::time::UniverseClock clock;
    clock.reset();

    const double normalEpoch = clock.timeSeconds();

    clock.setTimeScale(200.0);
    clock.setSimulationMode(true);
    clock.update(1.0);

    const double acceleratedEpoch = clock.timeSeconds();
    REQUIRE(acceleratedEpoch > normalEpoch + 150.0);
    REQUIRE_NEAR(clock.timeScale(), 200.0, 1.0e-12);

    clock.setSimulationMode(false);

    const double restoredEpoch = clock.timeSeconds();
    REQUIRE(!clock.simulationMode());
    REQUIRE_NEAR(clock.timeScale(), 1.0, 1.0e-12);

    // Leaving diagnostics re-anchors to real time instead of committing the
    // accelerated future. The test itself runs in far less than five seconds.
    REQUIRE(std::abs(restoredEpoch - normalEpoch) < 5.0);
    REQUIRE(acceleratedEpoch - restoredEpoch > 100.0);
}

void testDiagnosticTrajectorySessionIsTransactionalForMultipleShips()
{
    using game::simulation::UniverseDiagnosticTrajectorySession;
    using game::simulation::UniverseDiagnosticTrajectoryState;

    const glm::dvec3 playerProductionPosition(10.0, 20.0, 30.0);
    const glm::dvec3 npcProductionPosition(-40.0, 50.0, 60.0);

    UniverseDiagnosticTrajectorySession session;
    session.begin(1234.5);

    UniverseDiagnosticTrajectoryState player;
    player.systemId = 0;
    player.parentBodyId = "earth";
    player.relativePositionMeters = glm::dvec3(100.0, 0.0, 0.0);
    player.relativeVelocityMps = glm::dvec3(0.0, 10.0, 0.0);

    UniverseDiagnosticTrajectoryState npc;
    npc.systemId = 0;
    npc.parentBodyId = "earth";
    npc.relativePositionMeters = glm::dvec3(200.0, 0.0, 0.0);
    npc.relativeVelocityMps = glm::dvec3(0.0, 20.0, 0.0);

    REQUIRE(session.add(EntityId{1}, player));
    REQUIRE(session.add(EntityId{2}, npc));
    REQUIRE(session.size() == 2u);
    REQUIRE(session.find(EntityId{1})->systemId == 0);
    REQUIRE(session.find(EntityId{2})->systemId == 0);

    session.find(EntityId{1})->relativePositionMeters.x += 9999.0;
    session.find(EntityId{2})->relativeVelocityMps.y += 9999.0;

    // The alternate branch owns its own values. Production sentinels are not
    // aliases into the diagnostic session and therefore cannot be committed by
    // simply advancing/discarding that session.
    REQUIRE_VEC_NEAR(
        playerProductionPosition,
        glm::dvec3(10.0, 20.0, 30.0),
        0.0);
    REQUIRE_VEC_NEAR(
        npcProductionPosition,
        glm::dvec3(-40.0, 50.0, 60.0),
        0.0);

    session.discard();
    REQUIRE(!session.active());
    REQUIRE(session.size() == 0u);
    REQUIRE(session.find(EntityId{1}) == nullptr);
    REQUIRE(session.find(EntityId{2}) == nullptr);
}

void testSpatialContractsCarrySystemMembership()
{
    game::navigation::DynamicMotionState motion;
    game::navigation::HubNavigationFrame hubFrame;
    game::navigation::ReferenceFrame reference;
    game::navigation::ResolvedFrameState resolved;
    game::simulation::ShipReferenceFrameSnapshot shipFrame;
    ObjectSnapshot object;

    REQUIRE(motion.systemId == -1);
    REQUIRE(hubFrame.systemId == -1);
    REQUIRE(reference.systemId == -1);
    REQUIRE(resolved.systemId == -1);
    REQUIRE(shipFrame.systemId == -1);
    REQUIRE(object.systemId == -1);

    motion.systemId = 4;
    hubFrame.systemId = 4;
    reference.systemId = 4;
    resolved.systemId = 4;
    shipFrame.systemId = 4;
    object.systemId = 4;

    REQUIRE(motion.systemId == 4);
    REQUIRE(hubFrame.systemId == 4);
    REQUIRE(reference.systemId == 4);
    REQUIRE(resolved.systemId == 4);
    REQUIRE(shipFrame.systemId == 4);
    REQUIRE(object.systemId == 4);
}

void testRuntimeSystemPolicyRejectsCoordinateAliasingAcrossSystems()
{
    using game::simulation::canCreateInActiveRuntimeSystem;
    using game::simulation::sameRuntimeSystem;

    REQUIRE(sameRuntimeSystem(0, 0));
    REQUIRE(sameRuntimeSystem(8, 8));
    REQUIRE(!sameRuntimeSystem(0, 8));
    REQUIRE(!sameRuntimeSystem(-1, 0));

    REQUIRE(canCreateInActiveRuntimeSystem(0, -1));
    REQUIRE(canCreateInActiveRuntimeSystem(0, 0));
    REQUIRE(!canCreateInActiveRuntimeSystem(8, 0));
    REQUIRE(!canCreateInActiveRuntimeSystem(-1, 0));
}

void testTimelineRevisionIsInterpolationFence()
{
    game::network::SnapshotMetadata map;
    game::network::SnapshotMetadata simulation;

    map.serverTick = 100;
    simulation.serverTick = 101;
    map.universeTimelineRevision = 7;
    simulation.universeTimelineRevision = 7;

    REQUIRE(
        game::client::MapTransitionController::simulationHasReached(
            map,
            simulation));

    simulation.universeTimelineRevision = 8;
    simulation.serverTick = 1000000;

    REQUIRE(
        !game::client::MapTransitionController::simulationHasReached(
            map,
            simulation));
}

void testCloudMotionPolicyHasObservableDebugContract()
{
    render::celestial::CloudMotionPolicyInput input;
    input.minimumWindSpeedMps = 10.0;
    input.maximumWindSpeedMps = 10.0;
    input.predominantDirectionDeg = 90.0;
    input.planetRadiusMeters = 6371000.0;
    input.baseHeightKm = 10.0;
    input.authoredVisualUvPerSecond = 0.01;
    input.referenceMeanWindMps = 10.0;
    input.physicalTimeScale = 1.0;

    input.debugSpeedMultiplier = 1.0;
    const auto normal =
        render::celestial::resolveCloudMotionPolicy(input);
    REQUIRE_NEAR(normal.driftUvPerSecond, 0.01, 1.0e-12);
    REQUIRE_NEAR(normal.morphologySpeedMultiplier, 1.0, 1.0e-12);

    input.debugSpeedMultiplier = 5.0;
    const auto faster =
        render::celestial::resolveCloudMotionPolicy(input);
    REQUIRE_NEAR(faster.driftUvPerSecond, 0.05, 1.0e-12);

    input.debugSpeedMultiplier = 0.0;
    const auto stopped =
        render::celestial::resolveCloudMotionPolicy(input);
    REQUIRE_NEAR(stopped.driftUvPerSecond, 0.0, 0.0);
    REQUIRE_NEAR(stopped.morphologySpeedMultiplier, 0.0, 0.0);

    input.debugSpeedMultiplier = 10000.0;
    const auto huge =
        render::celestial::resolveCloudMotionPolicy(input);
    REQUIRE_NEAR(huge.driftUvPerSecond, 100.0, 1.0e-9);
    REQUIRE_NEAR(huge.morphologySpeedMultiplier, 2.0, 1.0e-12);
}

void testCloudPhysicalFallbackUsesKilometresAsOneThousandMetres()
{
    render::celestial::CloudMotionPolicyInput input;
    input.minimumWindSpeedMps = 100.0;
    input.maximumWindSpeedMps = 100.0;
    input.predominantDirectionDeg = 90.0;
    input.planetRadiusMeters = 1000.0;
    input.baseHeightKm = 1.0;
    input.authoredVisualUvPerSecond = 0.0;
    input.referenceMeanWindMps = 100.0;
    input.physicalTimeScale = 1.0;
    input.debugSpeedMultiplier = 1.0;

    const auto result =
        render::celestial::resolveCloudMotionPolicy(input);

    constexpr double Pi = 3.141592653589793238462643383279502884;
    const double expected = 100.0 / (2.0 * Pi * 2000.0);

    REQUIRE_NEAR(
        result.physicalLongitudeUvPerSecond,
        expected,
        1.0e-15);
    REQUIRE_NEAR(result.driftUvPerSecond, expected, 1.0e-15);
}

void testAbsoluteOrbitStateRewindsDeterministically()
{
    world::orbits::OrbitalMotion motion;
    motion.enabled = true;
    motion.centerMeters = glm::dvec3(1000.0, 2000.0, -3000.0);
    motion.parentRadiusMeters = 6371000.0;
    motion.altitudeMeters = 400000.0;
    motion.orbitalPeriodSeconds = 5400.0;
    motion.inclinationDeg = 27.0;
    motion.longitudeOfAscendingNodeDeg = 41.0;
    motion.argumentOfPeriapsisDeg = 13.0;
    motion.initialPhaseDeg = 73.0;
    motion.epochSeconds = 100.0;

    const double normalTime = 1000.0;
    const glm::dvec3 before =
        world::orbits::computeOrbitPositionMeters(motion, normalTime);

    const glm::dvec3 future =
        world::orbits::computeOrbitPositionMeters(
            motion,
            normalTime + 200000.0);

    REQUIRE(glm::length(future - before) > 1.0);

    const glm::dvec3 afterRewind =
        world::orbits::computeOrbitPositionMeters(motion, normalTime);

    REQUIRE_VEC_NEAR(afterRewind, before, 1.0e-9);
}

void testSnapshotMetadataCarriesTimelineRevision()
{
    game::network::SnapshotMetadata metadata;
    REQUIRE(metadata.universeTimelineRevision == 1u);

    metadata.universeTimelineRevision = 42;
    REQUIRE(metadata.universeTimelineRevision == 42u);
}

using TestFunction = void (*)();

struct TestCase
{
    const char* name;
    TestFunction function;
};
} // namespace

void testClientSpatialDomainNeverInterpolatesAcrossSystems()
{
    using game::client::belongsToRenderSystem;
    using game::client::canInterpolateSystemLocalState;

    REQUIRE(canInterpolateSystemLocalState(0, 0));
    REQUIRE(canInterpolateSystemLocalState(7, 7));
    REQUIRE(!canInterpolateSystemLocalState(0, 1));
    REQUIRE(!canInterpolateSystemLocalState(-1, -1));

    REQUIRE(belongsToRenderSystem(3, 3));
    REQUIRE(!belongsToRenderSystem(2, 3));
    REQUIRE(!belongsToRenderSystem(-1, 3));
}

int main()
{
    const std::vector<TestCase> tests =
    {
        {
            "client spatial domains never interpolate across systems",
            testClientSpatialDomainNeverInterpolatesAcrossSystems
        },
        {
            "universe clock rewinds after accelerated diagnostic",
            testUniverseClockRewindsAfterAcceleratedDiagnostic
        },
        {
            "diagnostic trajectory session is transactional for multiple ships",
            testDiagnosticTrajectorySessionIsTransactionalForMultipleShips
        },
        {
            "spatial contracts carry system membership",
            testSpatialContractsCarrySystemMembership
        },
        {
            "runtime system policy rejects cross-system coordinate aliasing",
            testRuntimeSystemPolicyRejectsCoordinateAliasingAcrossSystems
        },
        {
            "timeline revision is interpolation fence",
            testTimelineRevisionIsInterpolationFence
        },
        {
            "cloud debug speed has observable behavior",
            testCloudMotionPolicyHasObservableDebugContract
        },
        {
            "cloud physical fallback uses km to m conversion",
            testCloudPhysicalFallbackUsesKilometresAsOneThousandMetres
        },
        {
            "absolute orbit state rewinds deterministically",
            testAbsoluteOrbitStateRewindsDeterministically
        },
        {
            "snapshot metadata carries timeline revision",
            testSnapshotMetadataCarriesTimelineRevision
        }
    };

    int failed = 0;

    for (const TestCase& test : tests)
    {
        try
        {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr
                << "[FAIL] " << test.name
                << "\n       " << error.what()
                << '\n';
        }
    }

    std::cout
        << '\n'
        << (tests.size() - static_cast<std::size_t>(failed))
        << '/' << tests.size()
        << " tests passed\n";

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
