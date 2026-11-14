#pragma once

// Ship / model ASSEMBLE as a library (EliteShipAssetAssemble and the mesh editor's "Сборка" share it).
// dir: a folder with ship_asset.json (schema elite.ship_asset_source.v1, tools/ships/ship_asset_source.py; the mesh
// editor writes the same schema from its assembly document) and the OBJ files it names. Builds the WORKING asset in
// metres, runs makeProductionCopy + validation and writes <productionPath> (+ .lodN.elmesh). Optional sections:
// states, compartments, openings, portals, collision, structural_links, hit_regions, sockets, object_frame.
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::ship_asset
{
// true on success; report: the one-line summary or the error
// the line edges the game's Elite / Technical styles draw (boundaries, creases > 30 deg, material seams, 3+ faces)
void buildLineEdges(elite::model_asset::MeshLod& m);

bool assembleShipAsset(const std::string& dir, const std::string& productionPath, const std::string& workingPath, std::string& report);
} // namespace elite::ship_asset
