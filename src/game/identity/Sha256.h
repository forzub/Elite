#pragma once

#include <cstddef>
#include <cstdint>

#include "src/game/identity/AuthToken.h"

namespace game::identity
{
AuthTokenDigest sha256Digest(const std::uint8_t* data, std::size_t size) noexcept;

inline AuthTokenDigest authTokenDigest(const AuthToken& token) noexcept
{
    return sha256Digest(token.bytes.data(), token.bytes.size());
}
}
