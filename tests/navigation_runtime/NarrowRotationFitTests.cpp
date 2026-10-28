#include "src/game/navigation/traffic/NarrowZoneEnvelope.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using namespace game::navigation::traffic;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// Cobra hull box: 26 m wide, 5 m high, 22.2 m long.
ShipHullBox cobra() { return {13.0, 2.5, 11.1}; }
constexpr double Tolerance = 5.0 * 3.14159265358979323846 / 180.0;
constexpr double SphereRadius = 17.3;

// A straight rectangular tube along +Z, up +Y, 3 m required clearance.
CompiledTrafficStage tube(double halfWidth, double halfHeight)
{
    CompiledTrafficStage stage;
    stage.kind = TrafficRouteStageKind::VolumeTransit;
    stage.volumeConstraint.policy =
        world::navigation::NavigationVolumePolicy::KeepInside;
    stage.volumeRequiredClearanceMeters = 3.0;
    for (double z : {0.0, 3000.0})
    {
        CompiledNavigationVolumeSection section;
        section.centerWorldMeters = {0.0, 0.0, z};
        section.forwardWorld = {0.0, 0.0, 1.0};
        section.upWorld = {0.0, 1.0, 0.0};
        section.crossSection =
            world::navigation::NavigationCrossSectionKind::Rectangle;
        section.halfWidthMeters = halfWidth;
        section.halfHeightMeters = halfHeight;
        stage.volumeSections.push_back(section);
    }
    return stage;
}

RotationFit rotationIn(const CompiledTrafficStage& stage, VolumeFit& fit)
{
    fit = analyseVolumeFit(stage, cobra(), SphereRadius, Tolerance);
    return analyseRotationFit(fit, cobra(), Tolerance,
        std::min(fit.marginRightMeters, fit.marginUpMeters));
}

// Flip between two walls: a wide, low tube (50 x 16 m). Turning about the
// tube's up keeps the long hull parallel to the walls and fits; turning
// about its right swings the nose across the low ceiling and does not.
void testFlipBetweenWallsUsesTheSafeAxis()
{
    VolumeFit fit;
    const auto rotation = rotationIn(tube(25.0, 8.0), fit);
    require(!fit.sphereFits && fit.alignedFits, "the fixture is not a narrow zone");
    require(rotation.yawFlipSafe, "the safe (yaw) flip was not found");
    require(!rotation.pitchFlipSafe, "the unsafe (pitch) flip was accepted");
    require(glm::length(rotation.flipAxisWorld - glm::dvec3(0.0, 1.0, 0.0)) < 1.0e-9,
        "the flip does not use the zone up axis");
    require(rotation.flipMarginMeters > 0.0, "no clearance left on the safe flip");
    // The swept hull of the chosen flip stays inside: half diagonal in the
    // swept plane plus the position tolerance within the room.
    require(std::hypot(11.1, 13.0) + std::min(fit.marginRightMeters, fit.marginUpMeters) <=
                fit.roomRightMeters,
        "the swept envelope of the chosen flip touches the wall");
}

// Both axes unsafe (a narrow, low tube): no flip is planned inside it.
void testNoSafeAxisMeansNoFlipInside()
{
    VolumeFit fit;
    const auto rotation = rotationIn(tube(17.0, 8.0), fit);
    require(!fit.sphereFits && fit.alignedFits, "the fixture is not a narrow zone");
    require(!rotation.yawFlipSafe && !rotation.pitchFlipSafe,
        "a flip axis was accepted although the swept hull hits a wall");
    require(glm::length(rotation.flipAxisWorld) == 0.0,
        "a flip axis was published for a zone that admits none");
}

// A tall narrow slot (roll 90): the zone frame follows the roll, and a flip
// about the zone up (the slot's long side) is the safe one.
void testRolledSlotFlipsAboutItsLongSide()
{
    VolumeFit fit;
    const auto rotation = rotationIn(tube(8.0, 25.0), fit);
    require(fit.rolled90, "the slot should be crossed rolled 90 degrees");
    require(rotation.yawFlipSafe && !rotation.pitchFlipSafe,
        "the rolled slot's safe flip axis was not identified");
    require(std::abs(glm::dot(rotation.flipAxisWorld, glm::dvec3(1.0, 0.0, 0.0))) > 0.999,
        "the rolled slot flips about the wrong axis");
}

} // namespace

int main()
{
    try
    {
        testFlipBetweenWallsUsesTheSafeAxis();
        testNoSafeAxisMeansNoFlipInside();
        testRolledSlotFlipsAboutItsLongSide();
        std::cout << "NARROW ROTATION FIT: PASS\n"
                  << " - flip between walls: safe yaw found, unsafe pitch rejected, swept hull inside\n"
                  << " - no safe axis: no flip inside the zone\n"
                  << " - rolled slot: flips about its long side\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NARROW ROTATION FIT: FAIL: " << error.what() << "\n";
        return 1;
    }
}
