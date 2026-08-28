#pragma once

#include <cstddef>
#include <set>
#include <string>

namespace elite::model_asset
{

// Stable authored IDs are human-readable and deterministic. The first owner keeps
// the preferred ID; later collisions receive a numeric suffix. Callers may choose
// a more descriptive preferred ID (for example "module.mesh") before falling back
// to this generic allocator.
inline std::string allocateStableId(
    const std::string& preferred,
    const std::string& fallback,
    const std::set<std::string>& used
)
{
    std::string base = preferred.empty() ? fallback : preferred;
    if (base.empty()) base = "item";
    if (used.find(base) == used.end()) return base;

    for (std::size_t suffix = 2;; ++suffix)
    {
        const std::string candidate = base + "." + std::to_string(suffix);
        if (used.find(candidate) == used.end()) return candidate;
    }
}

inline std::string allocateChildStableId(
    const std::string& parentId,
    const std::string& childPreferred,
    const std::string& role,
    const std::set<std::string>& used
)
{
    const std::string fallback = (parentId.empty() ? std::string("node") : parentId) + "." +
        (role.empty() ? std::string("child") : role);
    std::string preferred = childPreferred;
    if (preferred.empty() || used.find(preferred) != used.end())
    {
        if (childPreferred.empty() || childPreferred == parentId)
            preferred = fallback;
        else
            preferred = parentId + "." + childPreferred;
    }
    return allocateStableId(preferred, fallback, used);
}

} // namespace elite::model_asset
