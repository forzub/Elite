#pragma once

// Hit volumes of a hub city built from its FROZEN LAYOUT (src/model_asset/HubCityAssembly.h) - assembled the same way on
// the server and on the client, so they are NOT replicated (2026-11-23: the dedicated server's bootstrap snapshot was
// 68.8 MB - 66 MB of it ~300 cities' 12-17 thousand hit boxes - over the 16 MB wire limit: every remote client was
// dropped right after its welcome, "TCP peer closed connection"; the local game never encodes snapshots, so it worked).
// The client's route planner reads them as the city's obstacles: it gets them from clientLayoutCityHitVolumes(), built
// on a background thread the first time a city is asked for (nullptr until ready - the planner waits).
// A city with construction stages (a hub expansion plan) is assembled on the client too, by the client's construction
// clock (= the server's universe time) and the replicated construction overrides (audit DOC-1: this line said those
// stayed replicated - they do not since 2026-11-23).
// 2026-11-24 (audit SRV-2, made general): the same holds for every HUB KIT MODULE (district / mega prototypes, kit
// blocks, belt ring / drum / hotel) - its volumes come from its compiled package, which the client has: the district
// prototype alone was 17 226 boxes = 1.96 MB, the mega 1.07 MB, sent to every player joining Sol (3 MB a join).
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

// the compiled package of a hub kit module type ("assets/compiled/hub_modules/<folder>/<folder>.elmodel"), empty for
// other types - the one type -> package table of the server's spawn (GameSceneSetup) and the client
std::string hubKitPackagePath(ObjectType type);

// true: this object's hit volumes are the client's own (a layout city, or a hub kit module with its package)
bool layoutCityHitVolumesOnClient(ObjectType type, const std::string& hubId);

// the background assembly workers finished before the process exits (the application's shutdown calls it)
void joinClientGeometryWorkers();

// the client's copy (debug-hit-volume form, as the planner reads replicated ones); nullptr while it is being built
const std::vector<game::simulation::DebugHitVolumeSnapshot>* clientLayoutCityHitVolumes(ObjectType type,
                                                                                       const std::string& hubId);
} // namespace game::scene
