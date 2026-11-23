#pragma once

// Hit volumes of a hub city built from its FROZEN LAYOUT (src/model_asset/HubCityAssembly.h) - assembled the same way on
// the server and on the client, so they are NOT replicated (2026-11-23: the dedicated server's bootstrap snapshot was
// 68.8 MB - 66 MB of it ~300 cities' 12-17 thousand hit boxes - over the 16 MB wire limit: every remote client was
// dropped right after its welcome, "TCP peer closed connection"; the local game never encodes snapshots, so it worked).
// The client's route planner reads them as the city's obstacles: it gets them from clientLayoutCityHitVolumes(), built
// on a background thread the first time a city is asked for (nullptr until ready - the planner waits).
// A city with construction stages (a hub expansion plan) changes its volumes with the stages: those stay replicated.
#include <string>
#include <vector>

#include "src/game/damage/HitVolume.h"
#include "src/game/simulation/ObjectGraphSnapshot.h"
#include "src/model_asset/ModelAsset.h"
#include "src/world/types/ObjectType.h"

namespace game::scene
{
// the asset's enabled intact collision volumes as the object's hit volumes (the object type's mesh -> logical basis;
// a cylinder / capsule / sphere becomes its bounding box; the node's module group in subsystemId)
void collisionToHitVolumes(ObjectType type, const elite::model_asset::ModelAsset& asset,
                           std::vector<game::damage::HitVolume>& out);

// true: this city's hit volumes are the client's own (a layout city without construction stages)
bool layoutCityHitVolumesOnClient(ObjectType type, const std::string& hubId);

// the client's copy (debug-hit-volume form, as the planner reads replicated ones); nullptr while it is being built
const std::vector<game::simulation::DebugHitVolumeSnapshot>* clientLayoutCityHitVolumes(ObjectType type,
                                                                                       const std::string& hubId);
} // namespace game::scene
