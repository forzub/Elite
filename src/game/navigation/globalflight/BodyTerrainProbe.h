#pragma once

// The ground under a direction from a body's centre, as RENDERED (the flight renderer's surface field):
// one probe for the server's contact and the client's landing autopilot (NAVIGATION_V2 §1 - the
// autopilots run on the client and need the same ground the server touches). The body's surface field
// loads in the background on the first probe; until then the probe answers false (unknown). The field
// cache is process-wide: an embedded server and its client share one field per body.
#include <string>

#include <glm/glm.hpp>

#include "src/world/celestial/CelestialTypes.h"

namespace game::navigation::globalflight
{

struct BodyTerrainSample
{
    double heightM = 0.0;        // terrain or sea surface above the body radius (as rendered, ~1 m detail)
    double smoothHeightM = 0.0;  // the same on the 30 m relief (diagnostics)
    int material = -1;           // world::celestial::surface::SurfaceMaterial, TerrainWaterMaterial = sea / lake
    double slopeDeg = 0.0;
    int feature = 0;             // world::celestial::surface::FeatureClass
};
inline constexpr int TerrainWaterMaterial = 100;

bool sampleBodyTerrain(const world::celestial::CelestialSystemSnapshot& snapshot, const std::string& bodyId,
                       const glm::dvec3& worldDirection, BodyTerrainSample& ground);

} // namespace game::navigation::globalflight
