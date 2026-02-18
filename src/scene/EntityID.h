#pragma once
#include <cstdint>

struct EntityId
{
    uint32_t value;

    bool operator==(const EntityId& other) const
    {
        return value == other.value;
    }
};

namespace std
{
    template<>
    struct hash<EntityId>
    {
        size_t operator()(const EntityId& id) const noexcept
        {
            return std::hash<uint32_t>()(id.value);
        }
    };
}