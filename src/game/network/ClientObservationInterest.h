#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/scene/EntityID.h"

namespace game::network
{

// A presentation/interest hint from one client. It is deliberately incapable
// of requesting FullControl or reporting hit/collision outcomes.
enum class ClientObservationDetail : std::uint8_t
{
    Kinematic = 0,
    Response = 1
};

struct ClientObservationInterestEntry
{
    EntityId targetId {0};
    ClientObservationDetail detail = ClientObservationDetail::Kinematic;
};

inline constexpr std::size_t MaxClientObservationInterestEntries = 64;

struct ClientObservationInterest
{
    std::uint64_t serial = 0;
    std::vector<ClientObservationInterestEntry> entries;
};

} // namespace game::network
