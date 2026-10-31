#pragma once

#include <string>

namespace planet_lab
{
/*
    Planet Lab hosting the PRODUCTION celestial renderer
    (render::celestial_flight) on the real catalog (StarAtlasDatabase +
    CelestialSystemRuntime at a universe time): the same path the game
    uses, without a server.

      --flight                 interactive flight (keys in PlanetLabFlight.cpp)
      --flight-capture DIR     3 styles x targets x (far / orbit / approach /
                               surface) PNGs + a travel sequence Earth -> Moon
      --flight-bench DIR       per style and distance: CPU / GPU ms per pass,
                               memory, streaming (flight_bench.txt)
      --flight-check           angular size / position, ephemeris motion and
                               spin, local terrain determinism
*/
struct FlightOptions
{
    std::string root = ".";
    std::string captureDir;
    std::string benchDir;
    bool check = false;
    double universeTimeSeconds = 0.0;
};

int runFlight(const FlightOptions& options);
} // namespace planet_lab
