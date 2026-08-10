#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/PlayerSpatialDomainResolver.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/coordinates/WorldPosition.h"

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
    int line)
{
    throw TestFailure(
        std::string(file) + ':' + std::to_string(line) +
        ": check failed: " + expression
    );
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
        fail(expression, file, line);
}

#define REQUIRE_NEAR(actual, expected, epsilon) \
    requireNear((actual), (expected), (epsilon), #actual, __FILE__, __LINE__)

std::vector<world::celestial::StarSystemSummary> makeSystems()
{
    std::vector<world::celestial::StarSystemSummary> systems;

    world::celestial::StarSystemSummary sol;
    sol.id = 0;
    sol.name = "Sol";
    sol.positionLy = glm::dvec3(0.0, 0.0, 0.0);
    systems.push_back(sol);

    world::celestial::StarSystemSummary target;
    target.id = 1;
    target.name = "Target";
    target.positionLy = glm::dvec3(25.0, 4.0, -2.0);
    systems.push_back(target);

    return systems;
}

world::coordinates::WorldPosition localAu(
    const glm::dvec3& valueAu)
{
    return world::coordinates::makeWorldPositionFromMeters(
        valueAu * world::celestial::MetersPerAu
    );
}

void testInsideSourceSystemRemainsSystemLocal()
{
    const auto result =
        game::navigation::resolvePlayerSpatialDomain(
            makeSystems(),
            0,
            localAu(glm::dvec3(50.0, 0.0, 0.0)),
            100.0
        );

    REQUIRE(result.valid);
    REQUIRE(result.currentSystemId == 0);
    REQUIRE_NEAR(result.systemLocalAu.x, 50.0, 1.0e-9);
    REQUIRE_NEAR(result.systemLocalAu.y, 0.0, 1.0e-9);
    REQUIRE_NEAR(result.galacticPositionLy.x,
        50.0 * world::celestial::MetersPerAu *
            world::coordinates::LightYearsPerMeter,
        1.0e-12);
}

void testLeavingSystemBecomesInterstellarAbsolute()
{
    const auto result =
        game::navigation::resolvePlayerSpatialDomain(
            makeSystems(),
            0,
            localAu(glm::dvec3(150.0, 0.0, 0.0)),
            100.0
        );

    REQUIRE(result.valid);
    REQUIRE(result.currentSystemId == -1);
    REQUIRE_NEAR(glm::length(result.systemLocalAu), 0.0, 1.0e-12);

    const glm::dvec3 absoluteLy =
        world::coordinates::toGalacticLy(result.worldPosition);

    REQUIRE_NEAR(
        absoluteLy.x,
        150.0 * world::celestial::MetersPerAu *
            world::coordinates::LightYearsPerMeter,
        1.0e-12);
}

void testEnteringAnotherSystemRebasesLocalCoordinates()
{
    const auto systems = makeSystems();
    const glm::dvec3 targetPositionLy = systems[1].positionLy;
    const glm::dvec3 tenAuLy =
        glm::dvec3(10.0, 0.0, 0.0) *
        world::celestial::MetersPerAu *
        world::coordinates::LightYearsPerMeter;

    const glm::dvec3 fromSolLocalMeters =
        (targetPositionLy + tenAuLy) *
        world::coordinates::MetersPerLightYear;

    const auto result =
        game::navigation::resolvePlayerSpatialDomain(
            systems,
            0,
            world::coordinates::makeWorldPositionFromMeters(
                fromSolLocalMeters
            ),
            100.0
        );

    REQUIRE(result.valid);
    REQUIRE(result.currentSystemId == 1);
    REQUIRE_NEAR(result.systemLocalAu.x, 10.0, 1.0e-6);
    REQUIRE_NEAR(result.systemLocalAu.y, 0.0, 1.0e-6);
    REQUIRE_NEAR(result.systemLocalAu.z, 0.0, 1.0e-6);
}

void testInterstellarAbsoluteCanEnterKnownSystem()
{
    const auto systems = makeSystems();
    const glm::dvec3 absoluteLy =
        systems[1].positionLy +
        glm::dvec3(0.0, 20.0, 0.0) *
            world::celestial::MetersPerAu *
            world::coordinates::LightYearsPerMeter;

    const auto result =
        game::navigation::resolvePlayerSpatialDomain(
            systems,
            -1,
            world::coordinates::makeWorldPositionFromMeters(
                absoluteLy * world::coordinates::MetersPerLightYear
            ),
            100.0
        );

    REQUIRE(result.valid);
    REQUIRE(result.currentSystemId == 1);
    REQUIRE_NEAR(result.systemLocalAu.y, 20.0, 1.0e-6);
}

void testUnknownSourceSystemIsRejected()
{
    const auto result =
        game::navigation::resolvePlayerSpatialDomain(
            makeSystems(),
            999,
            localAu(glm::dvec3(0.0)),
            100.0
        );

    REQUIRE(!result.valid);
}
}

int main()
{
    try
    {
        testInsideSourceSystemRemainsSystemLocal();
        testLeavingSystemBecomesInterstellarAbsolute();
        testEnteringAnotherSystemRebasesLocalCoordinates();
        testInterstellarAbsoluteCanEnterKnownSystem();
        testUnknownSourceSystemIsRejected();

        std::cout << "Player spatial-domain contracts: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Player spatial-domain contracts: FAIL: "
            << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
