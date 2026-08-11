#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "src/game/client/ClientServerClock.h"
#include "src/game/client/ClientUniverseTimeline.h"
#include "src/game/client/ClientCelestialMapBridge.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/HubNavigationFrame.h"
#include "src/world/celestial/CelestialRuntimeRegistry.h"
#include "src/world/celestial/CelestialSystemRuntime.h"
#include "src/world/celestial/StarAtlasDatabase.h"

namespace
{

void require(
    bool condition,
    const char* expression,
    const char* file,
    int line
)
{
    if (condition)
        return;

    std::ostringstream message;
    message
        << file << ':' << line
        << ": requirement failed: "
        << expression;
    throw std::runtime_error(message.str());
}

#define REQUIRE(expr) \
    require(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const char* label
)
{
    if (std::abs(actual - expected) <= tolerance)
        return;

    std::ostringstream message;
    message
        << label
        << ": expected " << expected
        << ", got " << actual
        << ", tolerance " << tolerance;
    throw std::runtime_error(message.str());
}

void requireVectorNear(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double tolerance,
    const char* label
)
{
    const double error = glm::length(actual - expected);

    if (error <= tolerance)
        return;

    std::ostringstream message;
    message
        << label
        << ": vector error " << error
        << " exceeds " << tolerance;
    throw std::runtime_error(message.str());
}

world::celestial::StarAtlasDatabase loadAtlas()
{
    world::celestial::StarAtlasDatabase atlas;

    bool loaded = atlas.load("src/assets/data/galaxy_details");

    if (!loaded)
        loaded = atlas.load("assets/data/galaxy_details");

    REQUIRE(loaded);
    REQUIRE(!atlas.systems().empty());
    return atlas;
}

const world::celestial::CelestialBodyState* findBody(
    const world::celestial::CelestialSystemSnapshot& snapshot,
    const std::string& bodyId
)
{
    for (const auto& body : snapshot.bodies)
    {
        if (body.id == bodyId)
            return &body;
    }

    return nullptr;
}

void testClientUniverseTimelineUsesServerAnchor()
{
    game::client::ClientUniverseTimeline timeline;

    REQUIRE(!timeline.synchronized());

    timeline.synchronize(100.0, 1000.0, 4.0, 3);
    REQUIRE(timeline.synchronized());
    requireNear(
        timeline.timeAtServerTime(100.0),
        1000.0,
        1e-12,
        "timeline anchor"
    );
    requireNear(
        timeline.timeAtServerTime(100.25),
        1001.0,
        1e-12,
        "timeline scale"
    );

    timeline.synchronize(200.0, 2500.0, 0.0, 4);
    requireNear(
        timeline.timeAtServerTime(900.0),
        2500.0,
        1e-12,
        "paused timeline"
    );
}

void testRegistryIsDemandDriven()
{
    const auto atlas = loadAtlas();
    world::celestial::CelestialRuntimeRegistry registry;
    registry.initialize(atlas);

    REQUIRE(registry.cachedSystemCount() == 0);

    const int firstSystemId = atlas.systems().front().id;
    const auto* first = registry.resolve(firstSystemId, 12345.0);

    REQUIRE(first != nullptr);
    REQUIRE(first->systemId == firstSystemId);
    REQUIRE(!first->bodies.empty());
    REQUIRE(registry.cachedSystemCount() == 1);

    const auto* repeated = registry.resolve(firstSystemId, 12345.0);
    REQUIRE(repeated == first);
    REQUIRE(registry.cachedSystemCount() == 1);

    if (atlas.systems().size() > 1)
    {
        const int secondSystemId = atlas.systems()[1].id;
        const auto* second = registry.resolve(secondSystemId, 12345.0);
        REQUIRE(second != nullptr);
        REQUIRE(second->systemId == secondSystemId);
        REQUIRE(registry.cachedSystemCount() == 2);
    }
}

void testRegistryMatchesSharedRuntime()
{
    const auto atlas = loadAtlas();
    const int systemId = atlas.systems().front().id;
    const auto* definition = atlas.findSystem(systemId);
    REQUIRE(definition != nullptr);

    constexpr double TimeSeconds = 987654.321;

    world::celestial::CelestialSystemRuntime direct;
    direct.setSystem(definition);
    direct.update(TimeSeconds);

    world::celestial::CelestialRuntimeRegistry registry;
    registry.initialize(atlas);
    const auto* resolved = registry.resolve(systemId, TimeSeconds);

    REQUIRE(resolved != nullptr);
    REQUIRE(resolved->bodies.size() == direct.snapshot().bodies.size());

    for (const auto& directBody : direct.snapshot().bodies)
    {
        const auto* resolvedBody = findBody(*resolved, directBody.id);
        REQUIRE(resolvedBody != nullptr);

        requireVectorNear(
            resolvedBody->positionAu,
            directBody.positionAu,
            1e-15,
            "position parity"
        );

        requireVectorNear(
            resolvedBody->velocityAuPerSecond,
            directBody.velocityAuPerSecond,
            1e-20,
            "velocity parity"
        );

        requireNear(
            resolvedBody->rotationPhaseRad,
            directBody.rotationPhaseRad,
            1e-15,
            "rotation parity"
        );
    }
}

void testRequestedTimeChangesComputedState()
{
    const auto atlas = loadAtlas();
    const int systemId = atlas.systems().front().id;

    world::celestial::CelestialRuntimeRegistry registry;
    registry.initialize(atlas);

    const auto* atStart = registry.resolve(systemId, 0.0);
    REQUIRE(atStart != nullptr);

    world::celestial::CelestialSystemSnapshot startCopy = *atStart;

    const auto* later = registry.resolve(
        systemId,
        world::celestial::SecondsPerDay
    );
    REQUIRE(later != nullptr);
    requireNear(
        later->simTimeSeconds,
        world::celestial::SecondsPerDay,
        0.0,
        "resolved universe time"
    );

    bool foundMovingBody = false;

    for (const auto& startBody : startCopy.bodies)
    {
        const auto* laterBody = findBody(*later, startBody.id);
        REQUIRE(laterBody != nullptr);

        if (glm::length(laterBody->positionAu - startBody.positionAu) > 1e-12)
        {
            foundMovingBody = true;
            break;
        }
    }

    REQUIRE(foundMovingBody);
}


void testClientUniverseTimelineDoesNotCreateSecondClock()
{
    game::client::ClientUniverseTimeline timeline;

    timeline.synchronize(10.0, 1000.0, 2.0, 9);
    requireNear(
        timeline.timeAtServerTime(10.5),
        1001.0,
        1e-12,
        "initial mapping"
    );

    // A same-revision observation is validation only. It must not become a
    // new frame-driven universe clock.
    timeline.synchronize(11.0, 1002.0, 2.0, 9);
    requireNear(
        timeline.timeAtServerTime(12.0),
        1004.0,
        1e-12,
        "same revision mapping"
    );
}

void testClientCelestialMapBridgeOwnsPredictableRotationOnly()
{
    world::celestial::CelestialSystemSnapshot celestial;
    celestial.systemId = 7;
    celestial.simTimeSeconds = 123456.0;

    world::celestial::CelestialBodyState body;
    body.id = "planet.test";
    body.rotationPhaseRad = 1.25;
    body.dayLengthHours = 30.0;
    body.rotationDirection = -1;
    body.axialTiltDeg = 12.0;
    body.axisNodeDeg = 23.0;
    body.textureLongitudeOffsetDeg = 34.0;
    body.worldMeters = glm::dvec3(999.0, 888.0, 777.0);
    celestial.bodies.push_back(body);

    world::celestial::DetailMapSnapshot detail;
    detail.valid = true;
    detail.systemId = 7;
    detail.planetBodyId = body.id;
    detail.planetCenterMeters = glm::dvec3(10.0, 20.0, 30.0);

    REQUIRE(
        game::client::applyClientCelestialPresentation(
            detail,
            celestial
        )
    );

    requireNear(
        detail.universeTimeSeconds,
        celestial.simTimeSeconds,
        0.0,
        "detail client universe time"
    );
    requireNear(
        detail.planetRotationPhaseRad,
        body.rotationPhaseRad,
        0.0,
        "detail client rotation phase"
    );
    requireVectorNear(
        detail.planetCenterMeters,
        glm::dvec3(10.0, 20.0, 30.0),
        0.0,
        "bridge must not mix translation epochs"
    );

    world::celestial::HubMapSnapshot hub;
    hub.valid = true;
    hub.systemId = 7;
    hub.parentBodyId = body.id;
    hub.parentPlanetCenterLocalMeters = glm::dvec3(0.0, -1000.0, 0.0);

    REQUIRE(
        game::client::applyClientCelestialPresentation(
            hub,
            celestial
        )
    );

    requireNear(
        hub.parentPlanetRotationPhaseRad,
        body.rotationPhaseRad,
        0.0,
        "hub client rotation phase"
    );
    requireVectorNear(
        hub.parentPlanetCenterLocalMeters,
        glm::dvec3(0.0, -1000.0, 0.0),
        0.0,
        "hub bridge must preserve dynamic local geometry"
    );
}

void testRotatingHubFrameVelocityRoundTrip()
{
    game::navigation::HubNavigationFrame frame;
    frame.valid = true;
    frame.originMeters = glm::dvec3(100.0, 200.0, 300.0);
    frame.velocityMetersPerSecond = glm::dvec3(10.0, 20.0, 30.0);
    frame.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    frame.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    frame.normalAxis = glm::dvec3(0.0, 0.0, 1.0);
    frame.angularVelocityWorldRadPerSecond =
        glm::dvec3(0.0, 0.0, 0.01);

    const glm::dvec3 localPosition(1000.0, 200.0, -50.0);
    const glm::dvec3 localVelocity(3.0, -4.0, 5.0);

    const glm::dvec3 worldPosition =
        frame.localToWorldPosition(localPosition);

    const glm::dvec3 worldVelocity =
        frame.localToWorldVelocity(
            localPosition,
            localVelocity
        );

    const glm::dvec3 restoredLocalVelocity =
        frame.worldToLocalVelocity(
            worldPosition,
            worldVelocity
        );

    requireVectorNear(
        restoredLocalVelocity,
        localVelocity,
        1e-12,
        "rotating-frame velocity round trip"
    );

    const glm::dvec3 fixedPointWorldVelocity =
        frame.localToWorldVelocity(
            glm::dvec3(200.0, 0.0, 0.0),
            glm::dvec3(0.0)
        );

    requireVectorNear(
        fixedPointWorldVelocity,
        glm::dvec3(10.0, 22.0, 30.0),
        1e-12,
        "rotating-frame omega cross r term"
    );
}

void testHubTacticalIdleDoesNotFallTowardPlanet()
{
    game::navigation::HubNavigationFrame frame;
    frame.valid = true;
    frame.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    frame.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    frame.normalAxis = glm::dvec3(0.0, 0.0, 1.0);
    frame.angularVelocityWorldRadPerSecond =
        glm::dvec3(0.0, 0.0, 0.001);

    game::navigation::DynamicMotionState motion;
    motion.mode = game::navigation::MotionMode::HubTactical;
    motion.localPositionMeters = glm::dvec3(-10000.0, 2500.0, 0.0);
    motion.localVelocityMps = glm::dvec3(0.0);
    motion.gravityAccelerationMps2 = glm::dvec3(0.0, -9.8, 0.0);
    motion.engineAccelerationMps2 = glm::dvec3(0.0);

    world::coordinates::WorldPosition worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            frame.localToWorldPosition(motion.localPositionMeters)
        );

    const glm::dvec3 initialLocalPosition =
        motion.localPositionMeters;

    ShipParams params{};
    params.maxCombatSpeed = 350.0f;
    params.maxGs = 5.0f;

    game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
        motion,
        worldPosition,
        frame.kinematicFrame(),
        params,
        1.0
    );

    requireVectorNear(
        motion.localPositionMeters,
        initialLocalPosition,
        1e-12,
        "idle hub-local position"
    );
    requireVectorNear(
        motion.localVelocityMps,
        glm::dvec3(0.0),
        1e-12,
        "idle hub-local velocity"
    );
}

using TestFunction = void (*)();

struct TestCase
{
    const char* name;
    TestFunction function;
};

} // namespace

int main()
{
    const std::vector<TestCase> tests =
    {
        {
            "client universe timeline uses server anchor",
            testClientUniverseTimelineUsesServerAnchor
        },
        {
            "client universe timeline does not create second clock",
            testClientUniverseTimelineDoesNotCreateSecondClock
        },
        {
            "client celestial map bridge owns predictable rotation only",
            testClientCelestialMapBridgeOwnsPredictableRotationOnly
        },
        {
            "rotating hub frame velocity round trip",
            testRotatingHubFrameVelocityRoundTrip
        },
        {
            "hub tactical idle does not fall toward planet",
            testHubTacticalIdleDoesNotFallTowardPlanet
        },
        {
            "celestial registry is demand driven",
            testRegistryIsDemandDriven
        },
        {
            "registry matches shared runtime",
            testRegistryMatchesSharedRuntime
        },
        {
            "requested time changes computed state",
            testRequestedTimeChangesComputedState
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
        catch (...)
        {
            ++failed;
            std::cerr
                << "[FAIL] " << test.name
                << "\n       unknown exception\n";
        }
    }

    std::cout
        << '\n'
        << (tests.size() - static_cast<std::size_t>(failed))
        << '/' << tests.size()
        << " tests passed\n";

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
