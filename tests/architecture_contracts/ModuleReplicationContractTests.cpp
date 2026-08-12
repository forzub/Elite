#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "src/game/client/ClientModuleViewBuilder.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] module replication contract: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    ModuleDescriptor descriptor;
    descriptor.moduleId = "engine_left";
    descriptor.parentModuleId = "ship_frame";
    descriptor.subsystemId = "propulsion";
    descriptor.maxHealth = 250.0f;
    descriptor.destructible = true;
    descriptor.detachable = true;
    descriptor.hangable = true;
    descriptor.destroyPolicy = ModuleDestroyPolicy::Detach;
    descriptor.detachPolicy = ModuleDetachPolicy::OnDestroyed;
    descriptor.attachmentType = ModuleAttachmentType::Structural;
    descriptor.meshPartIds = {"engine_left_shell", "engine_left_nozzle"};
    descriptor.supportModuleIds = {"ship_frame", "engine_mount"};
    descriptor.minSupportsForAttached = 2;
    descriptor.minSupportsForStable = 1;

    game::simulation::ObjectModuleSnapshot runtime;
    runtime.moduleId = "engine_left";
    runtime.state = 3;
    runtime.health = 41.5f;
    runtime.aliveSupportCount = 1;

    const auto views = game::client::buildModuleViews(
        std::vector<ModuleDescriptor>{descriptor},
        std::vector<game::simulation::ObjectModuleSnapshot>{runtime}
    );

    require(views.size() == 1, "one descriptor must yield one client view");

    const auto& view = views.front();

    // Static values must come from the local descriptor catalog.
    require(view.moduleId == descriptor.moduleId, "module id mismatch");
    require(view.parentModuleId == descriptor.parentModuleId, "parent must come from descriptor");
    require(view.subsystemId == descriptor.subsystemId, "subsystem must come from descriptor");
    require(std::abs(view.maxHealth - descriptor.maxHealth) < 0.001f, "max health must come from descriptor");
    require(view.destructible == descriptor.destructible, "destructible must come from descriptor");
    require(view.detachable == descriptor.detachable, "detachable must come from descriptor");
    require(view.hangable == descriptor.hangable, "hangable must come from descriptor");
    require(view.meshPartIds == descriptor.meshPartIds, "mesh-part ids must come from descriptor");
    require(view.supportModuleIds == descriptor.supportModuleIds, "support ids must come from descriptor");
    require(view.minSupportsForAttached == descriptor.minSupportsForAttached, "attached support threshold must come from descriptor");
    require(view.minSupportsForStable == descriptor.minSupportsForStable, "stable support threshold must come from descriptor");

    // Mutable values must come from the authoritative runtime snapshot.
    require(view.state == runtime.state, "runtime state mismatch");
    require(std::abs(view.health - runtime.health) < 0.001f, "runtime health mismatch");
    require(view.aliveSupportCount == runtime.aliveSupportCount, "runtime support count mismatch");

    const auto defaults = game::client::buildModuleViews(
        std::vector<ModuleDescriptor>{descriptor},
        {}
    );

    require(defaults.size() == 1, "missing runtime row must not erase static module definition");
    require(std::abs(defaults.front().health - descriptor.maxHealth) < 0.001f,
            "missing runtime row must use healthy descriptor default");

    std::cout << "[PASS] local descriptor + authoritative module runtime merge\n";
    return 0;
}
