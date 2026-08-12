#include <cmath>
#include <cstdlib>
#include <iostream>

#include "src/game/client/ClientCelestialMapBridge.h"

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        std::exit(1);
    }
}

bool near(double a, double b, double epsilon = 1.0e-9)
{
    return std::abs(a - b) <= epsilon;
}

bool nearVec(
    const glm::dvec3& a,
    const glm::dvec3& b,
    double epsilon = 1.0e-9
)
{
    return
        near(a.x, b.x, epsilon) &&
        near(a.y, b.y, epsilon) &&
        near(a.z, b.z, epsilon);
}

} // namespace

int main()
{
    using namespace world::celestial;

    CelestialSystemDefinition definition;
    definition.systemId = 17;
    definition.name = "Contract System";

    CelestialBodyDefinition star;
    star.id = "star";
    star.name = "Contract Star";
    star.type = BodyType::Star;
    star.radiusKm = 700000.0;
    star.staticPositionAu = {0.0, 0.0, 0.0};
    definition.bodies.push_back(star);

    CelestialBodyDefinition planet;
    planet.id = "planet";
    planet.name = "Contract Planet";
    planet.parentId = "star";
    planet.environmentPresetId = "contract_environment";
    planet.type = BodyType::Planet;
    planet.radiusKm = 6400.0;
    planet.distanceAu = 1.5;
    planet.orbitalPeriodDays = 420.0;
    planet.orbitalDirection = -1;
    planet.orbitalPhaseOffsetDeg = 45.0;
    planet.rotationOffsetDeg = 20.0;
    planet.dayLengthHours = 30.0;
    planet.rotationDirection = -1;
    planet.axialTiltDeg = 12.0;
    planet.axisNodeDeg = 24.0;
    planet.textureLongitudeOffsetDeg = 36.0;
    planet.ringPlaneInclinationOffsetDeg = 3.5;
    planet.ringVisual.displayProfile = "contract_rings";
    planet.ringVisual.renderMode = CelestialRingDisplayMode::ParticleCloud;

    CelestialRingDefinition ring;
    ring.name = "Main Ring";
    ring.innerRadiusKm = 8000.0;
    ring.outerRadiusKm = 16000.0;
    ring.composition = "ice";
    ring.render.opacity = 0.61f;
    ring.render.visibilityClass = CelestialRingVisibilityClass::Main;
    ring.render.displayMode = CelestialRingDisplayMode::ParticleCloud;
    planet.rings.push_back(ring);
    definition.bodies.push_back(planet);

    // A definition without runtime state must still survive via its authored
    // static position. This protects deterministic catalog fallback semantics.
    CelestialBodyDefinition authoredOnly;
    authoredOnly.id = "authored_only";
    authoredOnly.name = "Authored Only";
    authoredOnly.type = BodyType::AsteroidBelt;
    authoredOnly.staticPositionAu = {3.0, 4.0, 5.0};
    definition.bodies.push_back(authoredOnly);

    CelestialSystemSnapshot celestial;
    celestial.systemId = definition.systemId;
    celestial.systemName = definition.name;
    celestial.simTimeSeconds = 123456.0;

    CelestialBodyState starState;
    starState.id = "star";
    starState.positionAu = {0.25, 0.5, 0.75};
    celestial.bodies.push_back(starState);

    CelestialBodyState planetState;
    planetState.id = "planet";
    planetState.positionAu = {1.25, 2.5, 3.75};
    planetState.rotationPhaseRad = 1.125;
    planetState.dayLengthHours = 31.0;
    planetState.rotationDirection = 1;
    planetState.axialTiltDeg = 13.0;
    planetState.axisNodeDeg = 25.0;
    planetState.textureLongitudeOffsetDeg = 37.0;
    celestial.bodies.push_back(planetState);

    SystemMapSnapshot map;
    map.systemId = definition.systemId;
    map.universeTimeSeconds = celestial.simTimeSeconds;

    SystemMapObject dynamicObject;
    dynamicObject.stableId = "dynamic:keep-me";
    dynamicObject.positionAu = {9.0, 8.0, 7.0};
    map.objects.push_back(dynamicObject);

    require(
        game::client::rebuildSystemMapCelestialLayer(
            map,
            definition,
            celestial
        ),
        "client celestial layer rebuild rejected matching system"
    );

    require(map.bodies.size() == 3, "catalog body count was not preserved");
    require(map.objects.size() == 1, "dynamic map objects were modified");
    require(
        map.objects.front().stableId == "dynamic:keep-me",
        "dynamic map object identity was modified"
    );

    const auto& rebuiltStar = map.bodies[0];
    require(
        nearVec(rebuiltStar.positionAu, starState.positionAu),
        "runtime star position was not reconstructed locally"
    );

    const auto& rebuiltPlanet = map.bodies[1];
    require(
        nearVec(rebuiltPlanet.positionAu, planetState.positionAu),
        "runtime planet position was not reconstructed locally"
    );
    require(
        nearVec(rebuiltPlanet.orbitCenterAu, starState.positionAu),
        "planet orbit center is not the runtime parent position"
    );
    require(near(rebuiltPlanet.orbitRadiusAu, 1.5), "orbit radius changed");
    require(rebuiltPlanet.drawOrbit, "planet orbit visibility changed");
    require(
        near(rebuiltPlanet.rotationPhaseRad, planetState.rotationPhaseRad),
        "runtime rotation phase did not override authored phase"
    );
    require(
        near(rebuiltPlanet.dayLengthHours, planetState.dayLengthHours),
        "runtime day length did not override authored value"
    );
    require(
        rebuiltPlanet.rotationDirection == planetState.rotationDirection,
        "runtime rotation direction did not override authored value"
    );
    require(
        near(rebuiltPlanet.axialTiltDeg, planetState.axialTiltDeg),
        "runtime axial tilt did not override authored value"
    );
    require(
        rebuiltPlanet.environmentPresetId == "contract_environment",
        "static environment definition was lost"
    );
    require(
        rebuiltPlanet.ringVisual.displayProfile == "contract_rings" &&
        rebuiltPlanet.ringVisual.renderMode ==
            SystemMapRingDisplayMode::ParticleCloud,
        "ring visual profile was not reconstructed from the local definition"
    );
    require(
        rebuiltPlanet.rings.size() == 1 &&
        near(rebuiltPlanet.rings[0].innerRadiusKm, 8000.0) &&
        near(rebuiltPlanet.rings[0].outerRadiusKm, 16000.0) &&
        rebuiltPlanet.rings[0].visibilityClass ==
            SystemMapRingVisibilityClass::Main,
        "ring definition was not reconstructed from the local catalog"
    );

    const auto& rebuiltAuthored = map.bodies[2];
    require(
        nearVec(rebuiltAuthored.positionAu, authoredOnly.staticPositionAu),
        "body without runtime state lost its authored static position"
    );

    SystemMapSnapshot wrongEpoch = map;
    wrongEpoch.universeTimeSeconds += 1.0;
    require(
        !game::client::rebuildSystemMapCelestialLayer(
            wrongEpoch,
            definition,
            celestial
        ),
        "celestial state from a different universe epoch was accepted"
    );

    SystemMapSnapshot wrongDomain = map;
    wrongDomain.systemId = 999;
    require(
        !game::client::rebuildSystemMapCelestialLayer(
            wrongDomain,
            definition,
            celestial
        ),
        "cross-system celestial data was accepted into a System map"
    );

    std::cout
        << "[PASS] client System-map celestial composition preserves epoch/domain"
        << '\n';
    return 0;
}
