#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/presentation/GalaxyNavigationPresentation.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldFrame.h"
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

void requireVecNear(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double epsilon,
    const char* expression,
    const char* file,
    int line)
{
    if (glm::length(actual - expected) > epsilon)
        fail(expression, file, line);
}

#define REQUIRE_VEC_NEAR(actual, expected, epsilon) \
    requireVecNear((actual), (expected), (epsilon), #actual, __FILE__, __LINE__)

world::celestial::GalaxyMapSnapshot makeGalaxy()
{
    world::celestial::GalaxyMapSnapshot galaxy;

    world::celestial::GalaxyMapSystem sol;
    sol.id = 0;
    sol.name = "Sol";
    sol.positionLy = glm::dvec3(0.0);
    galaxy.systems.push_back(sol);

    world::celestial::GalaxyMapSystem remote;
    remote.id = 17;
    remote.name = "Remote";
    remote.positionLy = glm::dvec3(12.5, -4.25, 2.0);
    galaxy.systems.push_back(remote);

    return galaxy;
}

void testGalacticSystemAndRenderSpacesStayDistinct()
{
    const auto galaxy = makeGalaxy();

    world::celestial::PlayerNavigationState navigation;
    navigation.currentSystemId = 17;
    navigation.systemLocalAu = glm::dvec3(10.0, -20.0, 5.0);
    navigation.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            navigation.systemLocalAu * world::celestial::MetersPerAu
        );

    const auto marker =
        game::presentation::resolveGalaxyPlayerMarkerPosition(
            galaxy,
            navigation
        );

    const glm::dvec3 expectedGalactic =
        galaxy.systems[1].positionLy +
        navigation.systemLocalAu /
            game::navigation::SystemNavigationGrid::AuPerLightYear;

    REQUIRE(marker.insideKnownSystem);
    REQUIRE_VEC_NEAR(marker.positionLy, expectedGalactic, 1.0e-12);

    // The render frame is intentionally system-local and player-relative.
    // The galactic system root must never be added to OpenGL model positions.
    const auto playerLocal = navigation.worldPosition;
    const auto objectLocal = world::coordinates::translated(
        playerLocal,
        glm::dvec3(1250.0, -40.0, 80.0)
    );

    const auto renderFrame =
        world::coordinates::makeRenderFrameFromCamera(playerLocal);

    REQUIRE_VEC_NEAR(
        glm::dvec3(world::coordinates::toRenderLocal(playerLocal, renderFrame)),
        glm::dvec3(0.0),
        0.0
    );

    REQUIRE_VEC_NEAR(
        glm::dvec3(world::coordinates::toRenderLocal(objectLocal, renderFrame)),
        glm::dvec3(1250.0, -40.0, 80.0),
        1.0e-4
    );
}

void testRenderOriginSurvivesGalacticCellBoundary()
{
    using namespace world::coordinates;

    WorldPosition player;
    player.localMeters = glm::dvec3(
        GalacticCellSizeM * 0.5 - 10.0,
        100.0,
        -200.0
    );
    normalize(player);

    const WorldPosition object =
        translated(player, glm::dvec3(30.0, 5.0, -7.0));

    REQUIRE(object.cell.x != player.cell.x);

    const WorldFrame frame = makeRenderFrameFromCamera(player);
    const glm::vec3 renderLocal = toRenderLocal(object, frame);

    REQUIRE_NEAR(renderLocal.x, 30.0, 1.0e-3);
    REQUIRE_NEAR(renderLocal.y, 5.0, 1.0e-3);
    REQUIRE_NEAR(renderLocal.z, -7.0, 1.0e-3);
}

void testSameSystemLocalCoordinatesCanBelongToDifferentGalacticSystems()
{
    const auto galaxy = makeGalaxy();

    world::celestial::PlayerNavigationState inSol;
    inSol.currentSystemId = 0;
    inSol.systemLocalAu = glm::dvec3(1.0, 2.0, 3.0);
    inSol.worldPosition = world::coordinates::makeWorldPositionFromMeters(
        inSol.systemLocalAu * world::celestial::MetersPerAu
    );

    auto remote = inSol;
    remote.currentSystemId = 17;

    const auto solMarker =
        game::presentation::resolveGalaxyPlayerMarkerPosition(galaxy, inSol);
    const auto remoteMarker =
        game::presentation::resolveGalaxyPlayerMarkerPosition(galaxy, remote);

    REQUIRE_VEC_NEAR(
        remoteMarker.positionLy - solMarker.positionLy,
        galaxy.systems[1].positionLy - galaxy.systems[0].positionLy,
        1.0e-12
    );

    // Render-local geometry inside either system is identical because the
    // player-local frame removes the large system root before float conversion.
    const auto solFrame =
        world::coordinates::makeRenderFrameFromCamera(inSol.worldPosition);
    const auto remoteFrame =
        world::coordinates::makeRenderFrameFromCamera(remote.worldPosition);
    const auto localObject = world::coordinates::translated(
        inSol.worldPosition,
        glm::dvec3(300.0, 0.0, -500.0)
    );

    REQUIRE_VEC_NEAR(
        glm::dvec3(world::coordinates::toRenderLocal(localObject, solFrame)),
        glm::dvec3(world::coordinates::toRenderLocal(localObject, remoteFrame)),
        0.0
    );
}

void testInterstellarNavigationPositionIsAlreadyGalacticAbsolute()
{
    const auto galaxy = makeGalaxy();

    const glm::dvec3 absoluteLy(31.25, -7.5, 9.125);

    world::celestial::PlayerNavigationState navigation;
    navigation.currentSystemId = -1;
    navigation.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            absoluteLy * world::coordinates::MetersPerLightYear
        );

    const auto marker =
        game::presentation::resolveGalaxyPlayerMarkerPosition(
            galaxy,
            navigation
        );

    REQUIRE(!marker.insideKnownSystem);
    REQUIRE_VEC_NEAR(marker.positionLy, absoluteLy, 1.0e-12);
}
}

int main()
{
    try
    {
        testGalacticSystemAndRenderSpacesStayDistinct();
        testRenderOriginSurvivesGalacticCellBoundary();
        testSameSystemLocalCoordinatesCanBelongToDifferentGalacticSystems();
        testInterstellarNavigationPositionIsAlreadyGalacticAbsolute();

        std::cout << "Coordinate-frame contracts: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Coordinate-frame contracts: FAIL: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
