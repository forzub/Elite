#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "src/game/navigation/HubSemanticAnchor.h"

namespace game::navigation
{

class HubSemanticAnchorCatalog
{
public:
    bool load(
        const std::string& path =
            "assets/data/navigation/hub_semantic_anchors.json"
    );

    const std::vector<HubSemanticAnchorDefinition>& anchorsForModule(
        const std::string& hubModuleId
    ) const noexcept;

    const HubSemanticAnchorDefinition* find(
        const std::string& hubModuleId,
        const std::string& anchorId
    ) const noexcept;

private:
    std::unordered_map<std::string, std::vector<HubSemanticAnchorDefinition>>
        m_byModule;
};

} // namespace game::navigation
