#pragma once

#include <cstdint>
#include <functional>
#include <ostream>

namespace game::identity
{
struct OrganizationId
{
    std::uint64_t value = 0;

    constexpr explicit operator bool() const noexcept
    {
        return value != 0;
    }

    friend constexpr bool operator==(OrganizationId a, OrganizationId b) noexcept
    {
        return a.value == b.value;
    }

    friend constexpr bool operator!=(OrganizationId a, OrganizationId b) noexcept
    {
        return !(a == b);
    }
};

inline std::ostream& operator<<(std::ostream& os, OrganizationId id)
{
    return os << id.value;
}
}

namespace std
{
template<>
struct hash<game::identity::OrganizationId>
{
    std::size_t operator()(game::identity::OrganizationId id) const noexcept
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
}

using OrganizationId = game::identity::OrganizationId;
