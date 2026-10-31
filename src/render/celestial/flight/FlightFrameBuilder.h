#pragma once

#include <string>
#include <unordered_map>

#include "src/render/celestial/flight/CelestialFlightTypes.h"
#include "src/world/celestial/CelestialTypes.h"

namespace render::celestial_flight
{
/*
    Map and flight describe ONE world: the frame of the runtime renderer is
    built from the celestial snapshot the maps draw (positions, velocities,
    spin phase at universe time) and the system definition the atlas
    loaded; body orientation goes through the maps' own
    makePlanetTextureOrientation. Nothing here invents a position or size.

    Assets: a body with curated maps (assets/source|generated/celestial/
    <system>/<body>/...) is drawn from them (ImportedRealData), any other
    body procedurally from its generated descriptor (sourcePath).
*/
class FlightFrameBuilder
{
public:
    // assetRoot: directory holding assets/ (the game / Lab working dir).
    explicit FlightFrameBuilder(std::string assetRoot = ".");

    void build(const world::celestial::CelestialSystemDefinition& system,
               const world::celestial::CelestialSystemSnapshot& snapshot, const glm::dvec3& cameraPositionM,
               double worldTimeSeconds, FlightCelestialFrame& out);

private:
    struct Imported
    {
        bool checked = false;
        std::string albedo, height;
        double rangeM = 0.0;
        bool heightSigned = true;
        bool waterFromHeight = false;
    };
    const Imported& imported(const std::string& systemName, const std::string& sourcePath);

    std::string m_root;
    std::unordered_map<std::string, Imported> m_imported;
};

// System folder of curated assets ("Sol" -> "sol").
std::string celestialAssetFolder(const std::string& name);
} // namespace render::celestial_flight
