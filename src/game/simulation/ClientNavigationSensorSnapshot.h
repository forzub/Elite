#pragma once

#include "src/game/equipment/radar/RadarSensorTypes.h"
#include "src/game/navigation/NavigationSolution.h"

namespace game::simulation
{

struct ClientNavigationSensorSnapshot
{
    bool radarInstalled = false;
    bool radarOperational = false;

    // Repeated transport of the same scan is not a new measurement. Consumers
    // must key ingestion by scanSequence; it changes only on a discrete scan.
    bool hasRadarScan = false;
    game::radar::RadarScanReport latestRadarScan;

    bool hasNavigationSolution = false;
    game::navigation::NavigationSolution navigationSolution;
};

} // namespace game::simulation
