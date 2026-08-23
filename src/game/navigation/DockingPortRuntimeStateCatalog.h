#pragma once

#include <string>
#include <unordered_map>

#include "src/game/navigation/DockingCompatibility.h"

namespace game::navigation
{

/*
    Temporary diagnostic source for Hub docking-service state.

    This deliberately uses the same runtime DTO that the future session-scoped
    server docking service will publish.  Only the producer is temporary; map,
    navigation and guidance code must not depend on this catalog implementation.
*/
class DockingPortRuntimeStateCatalog
{
public:
    bool load(
        const std::string& path =
            "assets/data/navigation/hub_docking_runtime_test.json"
    );

    const DockingPortRuntimeState* find(
        const std::string& hubModuleId,
        const std::string& anchorId
    ) const noexcept;

private:
    static std::string key(
        const std::string& hubModuleId,
        const std::string& anchorId
    );

    std::unordered_map<std::string, DockingPortRuntimeState> m_states;
};

} // namespace game::navigation
