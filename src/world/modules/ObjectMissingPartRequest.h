#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace world::modules
{

struct ObjectMissingPartRequest
{
    std::string targetModuleId;

    std::string moduleClass;
    std::vector<std::string> acceptedReplacementTags;

    glm::mat4 homeLocalModel {1.0f};
};

inline bool replacementTagsCompatible(
    const std::vector<std::string>& accepted,
    const std::vector<std::string>& provided
)
{
    if (accepted.empty() || provided.empty())
        return false;

    for (const auto& a : accepted)
    {
        for (const auto& p : provided)
        {
            if (a == p)
                return true;
        }
    }

    return false;
}

} // namespace world::modules