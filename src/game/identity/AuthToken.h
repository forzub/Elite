#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace game::identity
{
inline constexpr std::size_t AuthTokenBytes = 32;

struct AuthToken
{
    std::array<std::uint8_t, AuthTokenBytes> bytes {};

    bool valid() const noexcept
    {
        for (const auto byte : bytes)
        {
            if (byte != 0)
                return true;
        }
        return false;
    }

    friend bool operator==(const AuthToken& a, const AuthToken& b) noexcept
    {
        return a.bytes == b.bytes;
    }

    friend bool operator!=(const AuthToken& a, const AuthToken& b) noexcept
    {
        return !(a == b);
    }
};

struct AuthTokenDigest
{
    std::array<std::uint8_t, AuthTokenBytes> bytes {};

    bool valid() const noexcept
    {
        for (const auto byte : bytes)
        {
            if (byte != 0)
                return true;
        }
        return false;
    }

    friend bool operator==(const AuthTokenDigest& a, const AuthTokenDigest& b) noexcept
    {
        return a.bytes == b.bytes;
    }

    friend bool operator!=(const AuthTokenDigest& a, const AuthTokenDigest& b) noexcept
    {
        return !(a == b);
    }
};
}
