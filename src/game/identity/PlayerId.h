#pragma once

#include <cstdint>
#include <functional>
#include <ostream>

namespace game::identity
{
struct PlayerId
{
    std::uint64_t value = 0;

    constexpr explicit operator bool() const noexcept
    {
        return value != 0;
    }

    friend constexpr bool operator==(PlayerId a, PlayerId b) noexcept
    {
        return a.value == b.value;
    }

    friend constexpr bool operator!=(PlayerId a, PlayerId b) noexcept
    {
        return !(a == b);
    }
};

inline std::ostream& operator<<(std::ostream& os, PlayerId id)
{
    return os << id.value;
}
}

namespace std
{
template<>
struct hash<game::identity::PlayerId>
{
    std::size_t operator()(game::identity::PlayerId id) const noexcept
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
}

// Transitional source-compatibility alias. New code should prefer the
// namespace-qualified type so PlayerId cannot be confused with EntityId.
using PlayerId = game::identity::PlayerId;
