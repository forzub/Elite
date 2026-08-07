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

#include "src/game/client/ClientUniverseClock.h"
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

void testClientUniverseClockUsesServerAnchor()
{
    game::client::ClientUniverseClock clock;

    REQUIRE(!clock.synchronized());

    clock.advance(10.0);
    requireNear(clock.timeSeconds(), 0.0, 0.0, "unsynchronized time");

    clock.synchronize(1000.0, 4.0);
    REQUIRE(clock.synchronized());
    requireNear(clock.timeSeconds(), 1000.0, 0.0, "synchronized time");
    requireNear(clock.timeScale(), 4.0, 0.0, "synchronized scale");

    clock.advance(0.25);
    requireNear(clock.timeSeconds(), 1001.0, 1e-12, "advanced time");

    clock.advance(-100.0);
    requireNear(clock.timeSeconds(), 1001.0, 1e-12, "negative delta");

    clock.synchronize(2500.0, 0.0);
    clock.advance(50.0);
    requireNear(clock.timeSeconds(), 2500.0, 1e-12, "paused time");
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
            "client universe clock uses server anchor",
            testClientUniverseClockUsesServerAnchor
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
