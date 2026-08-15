#pragma once

#include <cstddef>
#include <cstdint>

namespace game::identity
{
bool fillSecureRandom(
    std::uint8_t* data,
    std::size_t size
) noexcept;
}
