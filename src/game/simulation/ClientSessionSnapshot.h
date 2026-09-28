#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "src/world/celestial/CelestialTypes.h"
#include "src/game/navigation/OwnedNavigationAsset.h"
#include "src/game/simulation/ClientNavigationSensorSnapshot.h"
#include "src/world/WorldParams.h"

namespace game::simulation
{

struct AutomaticDockingRoutePoint
{
    // Exact Hub-local point copied from the authoritative
    // AcceptedManeuverProgram executed by Follower.
    glm::dvec3 positionHubLocalMeters {0.0};
    glm::dvec3 forwardHubLocal {0.0, 0.0, -1.0};
    glm::dvec3 upHubLocal {0.0, 1.0, 0.0};
    double speedMps = 0.0;
};

struct ClientSessionSnapshot
{
    world::celestial::PlayerNavigationState playerNavigation;
    std::vector<game::navigation::OwnedNavigationAsset> ownedNavigationAssets;
    game::simulation::ClientNavigationSensorSnapshot navigationSensors;
    WorldParams predictionWorldParams {0.0f, 50.0f};
    // Per-session authority fact for the controlled entity. The client uses
    // this to fence local prediction across temporary server Autopilot handoffs.
    bool controlledEntityAutopilotActive = false;

    // Authoritative Automatic-docking corridor. HUD consumes this exact
    // AcceptedManeuverProgram geometry; it never reconstructs a second route.
    bool automaticDockingRouteValid = false;
    bool automaticDockingRouteFinalIngress = false;
    std::uint64_t automaticDockingRouteRequestSerial = 0;
    std::uint64_t automaticDockingRouteRevision = 0;
    int automaticDockingRouteSystemId = -1;
    std::string automaticDockingRouteHubId;
    std::vector<AutomaticDockingRoutePoint> automaticDockingRoute;
    // Last terminal Automatic docking result, scoped to this session's ship.
    // The serial disambiguates an old result from a newly issued request.
    std::uint64_t dockingResultSerial = 0;
    bool dockingResultSucceeded = false;
    std::string dockingResultReason;

    double universeTimeSeconds = 0.0;
    double universeTimeScale = 1.0;
    std::uint64_t universeTimelineRevision = 1;
    double configuredUniverseTimeScale = 10000.0;

    bool universeTimeSimulation = false;
    std::string universeDate;
};

} // namespace game::simulation
