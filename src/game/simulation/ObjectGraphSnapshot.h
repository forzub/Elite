#pragma once

#include <vector>

#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/game/simulation/StructuralLinkSnapshot.h"
#include "src/game/simulation/ObjectAssemblyModuleSnapshot.h"
#include "src/game/simulation/ObjectDetachedFragmentSnapshot.h"
#include "src/game/simulation/ObjectRepairJobSnapshot.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"

namespace game::simulation
{

struct ObjectGraphSnapshot
{
    // Explicit sparse payload flags.
    // false means: client must keep the previously received value.
    // true + empty vector means: authoritative empty graph/list.
    bool hasModules = false;
    bool hasStructuralLinks = false;
    bool hasDebugHitVolumes = false;

    // Usually static local assembly transforms. Send with structural graph updates.
    // Detached fragments / repair jobs are event payloads: send while non-empty
    // and once more as empty to clear the client state.
    bool hasAssemblyModules = false;
    bool hasDetachedFragments = false;
    bool hasRepairJobs = false;

    std::vector<ObjectModuleSnapshot> modules;
    std::vector<StructuralLinkSnapshot> structuralLinks;
    std::vector<ObjectAssemblyModuleSnapshot> assemblyModules;
    std::vector<ObjectDetachedFragmentSnapshot> detachedFragments;
    std::vector<ObjectRepairJobSnapshot> repairJobs;
    std::vector<DebugHitVolumeSnapshot> debugHitVolumes;
};

}
