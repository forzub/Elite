#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "src/game/shared/module/ModuleViewData.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/world/descriptors/ModuleDescriptor.h"

namespace game::client
{

/*
    Rehydrates a presentation/debug module view from two deliberately separate
    sources of truth:
      - ModuleDescriptor: deterministic static definition from the local catalog;
      - ObjectModuleSnapshot: authoritative per-instance runtime state.

    The server and client each own their local descriptor library. Only runtime
    state crosses the replication boundary.
*/
inline std::vector<game::shared::ModuleViewData> buildModuleViews(
    const std::vector<ModuleDescriptor>& descriptors,
    const std::vector<game::simulation::ObjectModuleSnapshot>& runtimeModules
)
{
    std::unordered_map<std::string, const game::simulation::ObjectModuleSnapshot*>
        runtimeById;
    runtimeById.reserve(runtimeModules.size());

    for (const auto& runtime : runtimeModules)
        runtimeById[runtime.moduleId] = &runtime;

    std::vector<game::shared::ModuleViewData> views;
    views.reserve(descriptors.size());

    for (const auto& descriptor : descriptors)
    {
        game::shared::ModuleViewData view;

        // Static fields are reconstructed locally and never replicated as part
        // of ObjectModuleSnapshot.
        view.moduleId = descriptor.moduleId;
        view.parentModuleId = descriptor.parentModuleId;
        view.subsystemId = descriptor.subsystemId;
        view.maxHealth = descriptor.maxHealth;
        view.destructible = descriptor.destructible;
        view.detachable = descriptor.detachable;
        view.hangable = descriptor.hangable;
        view.destroyPolicy = static_cast<int>(descriptor.destroyPolicy);
        view.detachPolicy = static_cast<int>(descriptor.detachPolicy);
        view.attachmentType = static_cast<int>(descriptor.attachmentType);
        view.meshPartIds = descriptor.meshPartIds;
        view.supportModuleIds = descriptor.supportModuleIds;
        view.minSupportsForAttached = descriptor.minSupportsForAttached;
        view.minSupportsForStable = descriptor.minSupportsForStable;

        const auto it = runtimeById.find(descriptor.moduleId);
        if (it != runtimeById.end() && it->second)
        {
            const auto& runtime = *it->second;
            view.state = runtime.state;
            view.health = runtime.health;
            view.aliveSupportCount = runtime.aliveSupportCount;
        }
        else
        {
            // The server currently publishes one runtime row per descriptor,
            // but a missing row must still yield the descriptor's healthy
            // default instead of inventing another static network payload.
            view.state = 0;
            view.health = descriptor.maxHealth;
            view.aliveSupportCount = 0;
        }

        views.push_back(std::move(view));
    }

    return views;
}

} // namespace game::client
