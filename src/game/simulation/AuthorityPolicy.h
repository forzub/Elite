#pragma once

#include <cstdint>

namespace game::simulation
{

enum class AuthorityPolicy : std::uint8_t
{
    ServerAuthoritative = 0,
    ServerAuthoritativeWithClientPrediction,
    PresentationOnly
};

} // namespace game::simulation
