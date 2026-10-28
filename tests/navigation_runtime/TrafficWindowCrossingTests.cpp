#include "src/game/navigation/traffic/TrafficWindowCrossing.h"

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

// A straight BLUE block along +Z: portal "in" at z=0, portal "out" at
// z=1200, a 700 m inbound and a 150 m outbound protected straight, circle
// sections of radius 100 m with 22 m required clearance.
CompiledTrafficRoute block()
{
    CompiledTrafficRoute route;
    route.valid = true;
    CompiledTrafficStage free;
    free.kind = TrafficRouteStageKind::FreeSpace;
    free.toPortalId = "in";
    free.toWorldMeters = {0.0, 0.0, 0.0};
    route.stages.push_back(free);

    CompiledTrafficStage stage;
    stage.kind = TrafficRouteStageKind::VolumeTransit;
    stage.fromPortalId = "in";
    stage.toPortalId = "out";
    stage.fromWorldMeters = {0.0, 0.0, 0.0};
    stage.toWorldMeters = {0.0, 0.0, 1200.0};
    stage.volumeConstraint.policy =
        world::navigation::NavigationVolumePolicy::KeepInside;
    stage.volumeRequiredClearanceMeters = 22.0;
    for (double z : {0.0, 1200.0})
    {
        CompiledNavigationVolumeSection section;
        section.centerWorldMeters = {0.0, 0.0, z};
        section.forwardWorld = {0.0, 0.0, 1.0};
        section.upWorld = {0.0, 1.0, 0.0};
        section.radiusMeters = 100.0;
        stage.volumeSections.push_back(section);
    }
    stage.requiredCenterlineMeters = {
        {0.0, 0.0, 0.0}, {0.0, 0.0, 1200.0}};
    route.stages.push_back(stage);

    CompiledMandatoryTangentStraight in;
    in.portalId = "in";
    in.inbound = true;
    in.startWorldMeters = {0.0, 0.0, -700.0};
    in.endWorldMeters = {0.0, 0.0, 0.0};
    in.forwardWorld = {0.0, 0.0, 1.0};
    in.lengthMeters = 700.0;
    CompiledMandatoryTangentStraight out;
    out.portalId = "out";
    out.inbound = false;
    out.startWorldMeters = {0.0, 0.0, 1200.0};
    out.endWorldMeters = {0.0, 0.0, 1350.0};
    out.forwardWorld = {0.0, 0.0, 1.0};
    out.lengthMeters = 150.0;
    route.mandatoryTangentStraights = {in, out};
    route.requiredViaPointsMeters = {{0.0, 0.0, 0.0}, {0.0, 0.0, 1200.0}};
    return route;
}

// Approaching obliquely from close by: the crossing moves toward the
// approach side, inside the eroded window, and the whole block moves with
// it (both straights, both portals, the free stage's end, via points).
void testObliqueShortApproachMovesTheCrossing()
{
    auto route = block();
    const auto choices = chooseWindowCrossings(
        route, {400.0, 0.0, -1500.0}, {400.0, 0.0, 2500.0}, 18.0);
    require(choices.size() == 1, "one block expected");
    const auto& c = choices.front();
    const double offset = glm::length(c.offsetWorldMeters);
    require(offset > 1.0, "the crossing stayed centred on an oblique approach");
    require(offset <= c.admissibleRadiusMeters + 1.0e-6,
        "the crossing left the admissible window");
    require(c.admissibleRadiusMeters <= 0.9 * (100.0 - 22.0 - 18.0) + 1.0e-6,
        "the admissible window ignores clearance or ship size");
    require(c.offsetWorldMeters.x > 0.0,
        "the crossing moved away from the approach side");
    require(c.turnBeforeRad + c.turnAfterRad < c.centredTurnRad,
        "the chosen crossing does not reduce the turning");
    const glm::dvec3 d = c.offsetWorldMeters;
    require(glm::length(route.stages[1].fromWorldMeters - d) < 1.0e-9 &&
            glm::length(route.stages[0].toWorldMeters - d) < 1.0e-9 &&
            glm::length(route.mandatoryTangentStraights[0].startWorldMeters -
                        (glm::dvec3(0.0, 0.0, -700.0) + d)) < 1.0e-9 &&
            glm::length(route.mandatoryTangentStraights[1].endWorldMeters -
                        (glm::dvec3(0.0, 0.0, 1350.0) + d)) < 1.0e-9 &&
            glm::length(route.requiredViaPointsMeters[1] -
                        (glm::dvec3(0.0, 0.0, 1200.0) + d)) < 1.0e-9,
        "the block was not shifted consistently");
    require(glm::length(route.stages[1].volumeSections[0].centerWorldMeters) <
                1.0e-9,
        "the volume itself was moved");
}

// Far legs: a few metres of offset buy almost no turning; clearance wins
// and the crossing stays centred.
void testFarLegsKeepTheCentre()
{
    auto route = block();
    const auto choices = chooseWindowCrossings(
        route, {3000.0, 0.0, -20000.0}, {-2000.0, 0.0, 25000.0}, 18.0);
    require(choices.size() == 1, "one block expected");
    require(glm::length(choices.front().offsetWorldMeters) < 1.0e-6,
        "a negligible turning gain moved the crossing to the window edge");
}

} // namespace

int main()
{
    try
    {
        testObliqueShortApproachMovesTheCrossing();
        testFarLegsKeepTheCentre();
        std::cout
            << "TRAFFIC WINDOW CROSSING: PASS\n"
            << " - an oblique short approach moves the crossing inside the window\n"
            << " - far legs keep the centred crossing (clearance wins)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "TRAFFIC WINDOW CROSSING: FAIL: " << error.what() << "\n";
        return 1;
    }
}
