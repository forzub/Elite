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
#include "src/game/client/ClientHubTacticalPrediction.h"
#include "src/game/client/HubFramePresentation.h"
#include "src/game/client/ReferenceFramePresentation.h"
#include "src/game/navigation/DynamicMotionState.h"
#include "src/game/navigation/HubNavigationFrame.h"
#include "src/game/navigation/ReferenceFrame.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/game/server/FixedStepControlQueue.h"
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

void testSnapshotMetadataEpochIsSelfContained()
{
    game::network::SnapshotMetadata metadata;
    metadata.serverTick = 101;
    metadata.serverTimeSeconds = 12.5;
    metadata.universeTimeSeconds = 42.0;
    metadata.universeTimelineRevision = 7;

    REQUIRE(metadata.serverTick == 101);
    REQUIRE_NEAR(metadata.serverTimeSeconds, 12.5, 1.0e-12);
    REQUIRE_NEAR(metadata.universeTimeSeconds, 42.0, 1.0e-12);
    REQUIRE(metadata.universeTimelineRevision == 7);
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

void testHubTacticalClientPredictionAdvancesLocalMotion()
{
    ShipTransform transform;
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 0;
    transform.motion.hubId = "hub";
    transform.motion.travelFrame.systemId = 0;
    transform.motion.travelFrame.frameId = "ship_travel_test";
    transform.motion.travelFrame.localToWorldBasis = glm::dmat3(1.0);
    transform.motion.travelFrame.valid = true;
    transform.motion.localPositionMeters = glm::dvec3(10.0, 20.0, 30.0);
    transform.motion.localVelocityMps = glm::dvec3(4.0, -2.0, 1.0);

    game::simulation::ShipReferenceFrameSnapshot frame;
    frame.systemId = 0;
    frame.frameId = "ship_travel_test";
    frame.hubId = "hub";
    frame.valid = true;
    frame.originMeters = glm::dvec3(1000.0, 2000.0, 3000.0);
    frame.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    frame.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    frame.normalAxis = glm::dvec3(0.0, 0.0, 1.0);
    transform.motion.travelFrame = frame.kinematicFrame();

    transform.setWorldPositionMeters(
        frame.localToWorldPosition(transform.motion.localPositionMeters)
    );

    ShipControlState control;
    ShipParams params{};
    params.maxCombatSpeed = 350.0f;
    params.maxGs = 5.0f;
    params.strafeAccel = 20.0f;
    params.strafeDamping = 6.0f;
    params.maxStrafeSpeed = 80.0f;
    params.throttleAccel = 5.0f;

    const bool predicted =
        game::client::predictHubTacticalMotion(
            transform,
            frame,
            params,
            control,
            0.25f
        );

    REQUIRE(predicted);
    REQUIRE_VEC_NEAR(
        transform.motion.localPositionMeters,
        glm::dvec3(11.0, 19.5, 30.25),
        1.0e-12
    );
    REQUIRE_VEC_NEAR(
        transform.fullWorldMeters(),
        frame.localToWorldPosition(
            transform.motion.localPositionMeters
        ),
        1.0e-12
    );

    auto wrongFrame = frame;
    wrongFrame.frameId = "other_travel_frame";
    const glm::dvec3 before = transform.motion.localPositionMeters;
    REQUIRE(
        !game::client::predictHubTacticalMotion(
            transform,
            wrongFrame,
            params,
            control,
            0.25f
        )
    );
    REQUIRE_VEC_NEAR(transform.motion.localPositionMeters, before, 0.0);
}

void testReferenceFramePresentationUsesRenderEpoch()
{
    game::simulation::ShipReferenceFrameSnapshot from;
    from.systemId = 0;
    from.hubId = "hub";
    from.valid = true;
    from.originMeters = glm::dvec3(0.0, 0.0, 0.0);
    from.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    from.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    from.normalAxis = glm::dvec3(0.0, 0.0, 1.0);
    from.localPositionMeters = glm::dvec3(10.0, 0.0, 0.0);
    from.universeTimeSeconds = 100.0;

    auto to = from;
    to.originMeters = glm::dvec3(100.0, 200.0, 300.0);
    // Rotate the orbital triad 90 degrees around +Y.
    to.progradeAxis = glm::dvec3(0.0, 0.0, -1.0);
    to.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    to.normalAxis = glm::dvec3(1.0, 0.0, 0.0);
    to.localPositionMeters = glm::dvec3(30.0, 0.0, 0.0);
    to.universeTimeSeconds = 102.0;

    const auto mid =
        game::client::interpolateReferenceFramePresentation(
            from,
            to,
            0.5
        );

    REQUIRE(mid.valid);
    REQUIRE_VEC_NEAR(mid.originMeters, glm::dvec3(50.0, 100.0, 150.0), 1.0e-9);
    REQUIRE_NEAR(mid.universeTimeSeconds, 101.0, 1.0e-12);
    REQUIRE_NEAR(glm::length(mid.progradeAxis), 1.0, 1.0e-12);
    REQUIRE_NEAR(glm::length(mid.radialAxis), 1.0, 1.0e-12);
    REQUIRE_NEAR(glm::length(mid.normalAxis), 1.0, 1.0e-12);
    REQUIRE_NEAR(glm::dot(mid.progradeAxis, mid.radialAxis), 0.0, 1.0e-12);
    REQUIRE_VEC_NEAR(
        mid.normalAxis,
        glm::normalize(glm::cross(mid.progradeAxis, mid.radialAxis)),
        1.0e-12
    );

    auto otherSystem = to;
    otherSystem.systemId = 1;
    const auto snapped =
        game::client::interpolateReferenceFramePresentation(
            from,
            otherSystem,
            0.5
        );
    REQUIRE(snapped.systemId == 1);
    REQUIRE_VEC_NEAR(snapped.originMeters, otherSystem.originMeters, 0.0);
}

void testHubAttachedPresentationUsesOneSharedFrameSample()
{
    game::simulation::HubAttachmentSnapshot attachment;
    attachment.systemId = 0;
    attachment.hubId = "hub";
    attachment.moduleId = "station";
    attachment.localOffsetMeters = glm::dvec3(120.0, 30.0, -45.0);
    attachment.localRotationDeg = glm::dvec3(0.0);
    attachment.inheritHubOrientation = true;
    attachment.valid = true;

    game::simulation::ShipReferenceFrameSnapshot frameA;
    frameA.systemId = 0;
    frameA.hubId = "hub";
    frameA.valid = true;
    frameA.originMeters = glm::dvec3(1000.0, 2000.0, 3000.0);
    frameA.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    frameA.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    frameA.normalAxis = glm::dvec3(0.0, 0.0, 1.0);

    game::simulation::ShipReferenceFrameSnapshot frameB = frameA;
    frameB.originMeters = glm::dvec3(9000.0, -4000.0, 7000.0);
    // Same orthonormal orbital triad rotated 90 degrees around +Y.
    frameB.progradeAxis = glm::dvec3(0.0, 0.0, -1.0);
    frameB.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    frameB.normalAxis = glm::dvec3(1.0, 0.0, 0.0);

    const auto poseA =
        game::client::resolveHubAttachedObjectPresentation(attachment, frameA);
    const auto poseB =
        game::client::resolveHubAttachedObjectPresentation(attachment, frameB);

    REQUIRE(poseA.valid);
    REQUIRE(poseB.valid);
    REQUIRE(game::client::canResolveHubLocalPosition(0, "hub", frameA));
    REQUIRE(!game::client::canResolveHubLocalPosition(0, "other_hub", frameA));
    REQUIRE(!game::client::canResolveHubLocalPosition(9, "hub", frameA));

    const glm::dvec3 playerLocal(-10000.0, 2500.0, 0.0);
    const glm::dvec3 playerA = frameA.localToWorldPosition(playerLocal);
    const glm::dvec3 playerB = frameB.localToWorldPosition(playerLocal);

    // Moving/rotating the shared frame must not change co-frame distance.
    REQUIRE_NEAR(
        glm::length(poseA.worldPositionMeters - playerA),
        glm::length(poseB.worldPositionMeters - playerB),
        1.0e-9
    );

    auto wrongHub = frameA;
    wrongHub.hubId = "other_hub";
    REQUIRE(
        !game::client::resolveHubAttachedObjectPresentation(
            attachment, wrongHub).valid
    );

    auto wrongSystem = frameA;
    wrongSystem.systemId = 7;
    REQUIRE(
        !game::client::resolveHubAttachedObjectPresentation(
            attachment, wrongSystem).valid
    );
}


void testFixedStepControlQueueAcknowledgesOnlyConsumedStep()
{
    game::server::FixedStepControlQueue queue;

    ShipControlState a;
    a.controlTick = 101;
    a.yawInput = -1.0f;

    ShipControlState b;
    b.controlTick = 102;
    b.yawInput = 0.25f;

    ShipControlState c;
    c.controlTick = 103;
    c.yawInput = 1.0f;

    using Result = game::server::FixedStepControlQueue::EnqueueResult;

    REQUIRE(queue.enqueue(a) == Result::Accepted);
    REQUIRE(queue.enqueue(b) == Result::Accepted);
    REQUIRE(queue.enqueue(c) == Result::Accepted);
    REQUIRE(queue.pendingCount() == 3u);
    REQUIRE(queue.lastReceivedTick() == 103u);
    REQUIRE(queue.lastProcessedTick() == 0u);

    ShipControlState consumed;
    REQUIRE(queue.consumeNext(consumed));
    REQUIRE(consumed.controlTick == 101u);
    REQUIRE_NEAR(consumed.yawInput, -1.0, 1.0e-9);
    REQUIRE(queue.lastProcessedTick() == 101u);
    REQUIRE(queue.pendingCount() == 2u);

    REQUIRE(queue.consumeNext(consumed));
    REQUIRE(consumed.controlTick == 102u);
    REQUIRE_NEAR(consumed.yawInput, 0.25, 1.0e-9);
    REQUIRE(queue.lastProcessedTick() == 102u);
    REQUIRE(queue.pendingCount() == 1u);

    REQUIRE(queue.enqueue(b) == Result::Stale);
    REQUIRE(queue.pendingCount() == 1u);

    REQUIRE(queue.consumeNext(consumed));
    REQUIRE(consumed.controlTick == 103u);
    REQUIRE(queue.lastProcessedTick() == 103u);
    REQUIRE(queue.pendingCount() == 0u);

    // Diagnostic branches do not predict gameplay input. They may discard the
    // remaining production queue, but the acknowledgement must move to the
    // newest discarded sequence so those commands cannot reappear on exit.
    ShipControlState d;
    d.controlTick = 104;
    ShipControlState e;
    e.controlTick = 105;
    REQUIRE(queue.enqueue(d) == Result::Accepted);
    REQUIRE(queue.enqueue(e) == Result::Accepted);
    REQUIRE(queue.discardPendingAndAcknowledgeNewest());
    REQUIRE(queue.pendingCount() == 0u);
    REQUIRE(queue.lastProcessedTick() == 105u);
}

int main()
{
    const std::vector<TestCase> tests =
    {
        {
            "fixed-step control acknowledgement follows consumed steps",
            testFixedStepControlQueueAcknowledgesOnlyConsumedStep
        },
        {
            "client spatial domains never interpolate across systems",
            testClientSpatialDomainNeverInterpolatesAcrossSystems
        },
        {
            "hub tactical client prediction advances local motion",
            testHubTacticalClientPredictionAdvancesLocalMotion
        },
        {
            "reference frame presentation uses render epoch",
            testReferenceFramePresentationUsesRenderEpoch
        },
        {
            "hub attached presentation uses one shared frame sample",
            testHubAttachedPresentationUsesOneSharedFrameSample
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
            "simulation snapshot metadata is a complete map epoch",
            testSnapshotMetadataEpochIsSelfContained
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
