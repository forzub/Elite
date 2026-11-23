#pragma once

// FAR LIGHTS of a model asset (author 2026-11-23: «в реальности огни видны очень далеко ... даже на 30 км какие-то
// огни все-равно могут быть заметны»; the LOD pop of the hub city was mostly its lights). The light sources are read
// from the asset's own LOD0 geometry - every emissive material (windows, beacons, floodlights, flares, nav lights) and
// the procedural window facades - binned into cells, so a source keeps shining as a point long after the LOD that drew
// its geometry is gone. Any asset gets them: hub cities, stations, ships.
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/model_asset/ModelAsset.h"
#include "src/model_asset/ModelAssetPresentation.h"

namespace elite::model_asset
{
enum class AssetLightKind : std::uint8_t
{
    Window = 0,
    Beacon = 1,
    Flood = 2,
    Flare = 3,
    Nav = 4,
    Facade = 5,   // the average glow of a window facade (its lit share)
    Other = 6,
};

struct AssetLight
{
    glm::vec3 position {0.0f};   // asset space
    glm::vec3 color {1.0f};      // normalized glow colour
    float power = 0.0f;          // the emitting area (m2) x the kind's brightness: apparent brightness = power / d2
    float areaM2 = 0.0f;         // the glowing area (when its geometry is big on screen the point steps aside)
    AssetLightKind kind = AssetLightKind::Other;
};

// the lights of one geometry (its own frame): emissive / facade triangles binned into cellMeters cells per material
std::vector<AssetLight> extractGeometryLights(const MeshLod& mesh, const std::vector<MaterialDefinition>& materials,
                                              float cellMeters = 12.0f, float facadeCellMeters = 24.0f);

// the lights of the whole asset in asset space: every LOD0 component render node's geometry lights, placed by the node
std::vector<AssetLight> extractAssetLights(const ModelAsset& asset, const presentation::Index& index);
} // namespace elite::model_asset
