#pragma once

#include <cstdint>
#include <functional>

struct DroneInstanceId
{
    std::uint64_t value = 0;

    constexpr DroneInstanceId() noexcept = default;
    constexpr explicit DroneInstanceId(std::uint64_t inValue) noexcept
        : value(inValue)
    {
    }

    constexpr explicit operator bool() const noexcept { return value != 0; }

    friend constexpr bool operator==(DroneInstanceId a, DroneInstanceId b) noexcept
    {
        return a.value == b.value;
    }

    friend constexpr bool operator!=(DroneInstanceId a, DroneInstanceId b) noexcept
    {
        return !(a == b);
    }
};

namespace std
{
template<>
struct hash<DroneInstanceId>
{
    size_t operator()(DroneInstanceId id) const noexcept
    {
        return hash<std::uint64_t>{}(id.value);
    }
};
}
