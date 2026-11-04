#pragma once

#include <memory>
#include <string>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/SurfaceField.h"

// One loader of a body's surface field for every consumer: the flight
// renderer draws the terrain from it and the server collides ships with the
// SAME heights (curated real-data maps where they exist - Earth's DEM with
// its sea mask - else the procedural PlanetSurfaceField of the generated
// body). Moved out of CelestialFlightRenderer unchanged.
namespace world::celestial::surface
{
struct BodySurfaceSource
{
    std::string id;
    std::string descriptorPath;         // generated body JSON
    double radiusM = 0.0;
    std::string importedAlbedoPath;     // curated maps (empty = procedural)
    std::string importedHeightPath;
    double importedHeightRangeM = 4000.0;
    bool importedHeightSigned = true;
    bool importedWaterFromHeight = false;
};

// The curated maps of a body as the flight renderer finds them:
// <assetRoot>/assets/source/celestial/<system>/<body>/albedo_source.* and
// height_source.png (or the generated global albedo).
void findImportedBodyMaps(const std::string& assetRoot, const std::string& systemName, const std::string& sourcePath,
                          BodySurfaceSource& inOut);

// The asset folder name of a system / body ("Sol (Солнце)" -> "sol").
std::string celestialAssetFolder(const std::string& name);

struct LoadedBodySurface
{
    BodySurfaceDescriptor descriptor;
    std::shared_ptr<SurfaceField> field;
    std::string error;
};

// Blocking (image decode + field build): call it off the frame / tick thread.
bool loadBodySurface(const BodySurfaceSource& source, LoadedBodySurface& out);
} // namespace world::celestial::surface
