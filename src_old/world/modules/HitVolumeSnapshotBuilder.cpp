#include "HitVolumeSnapshotBuilder.h"

namespace world::modules
{

std::vector<game::simulation::DebugHitVolumeSnapshot>
HitVolumeSnapshotBuilder::build(const game::damage::HitComponent& hitComponent)
{
    std::vector<game::simulation::DebugHitVolumeSnapshot> out;
    out.reserve(hitComponent.volumes.size());

    for (const auto& v : hitComponent.volumes)
    {
        game::simulation::DebugHitVolumeSnapshot s;
        if (v.supportLinkVolume && v.destroyed)
            continue;
        s.moduleId = v.moduleId;
        s.subsystemId = v.subsystemId;
        s.layerIndex = v.layerIndex;
        s.priority = v.priority;
        s.center = v.center;
        s.halfSize = v.halfSize;
        s.orientation = v.orientation;
        s.destructible = v.destructible;
        s.destroyed = v.destroyed;
        s.health = v.health;
        s.maxHealth = v.maxHealth;

        s.supportLinkVolume = v.supportLinkVolume;
        s.supportLinkId = v.supportLinkId;
        s.supportModuleId = v.supportModuleId;

        out.push_back(std::move(s));
    }

    return out;
}

} // namespace world::modules     