#pragma once
#include <cstdint>
#include <functional>
#include <ostream>

struct EntityId
{
    uint32_t value;

    bool operator==(const EntityId& other) const
    {
        return value == other.value;
    }
};

// 👇 ВНЕ структуры
inline std::ostream& operator<<(std::ostream& os, const EntityId& id)
{
    return os << id.value;
}

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