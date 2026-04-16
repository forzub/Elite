#pragma once

#include <vector>

#include "src/game/damage/HitComponent.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"

namespace world::modules
{

class HitVolumeSnapshotBuilder
{
public:
    static std::vector<game::simulation::DebugHitVolumeSnapshot> build(
        const game::damage::HitComponent& hitComponent
    );
};

} // namespace world::modules