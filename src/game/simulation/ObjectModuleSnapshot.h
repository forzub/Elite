#pragma once

#include <cstdint>
#include <string>

namespace game::simulation
{

/*
    Replicated per-instance module state only.

    Static module definition data (parent/subsystem, health limits, policies,
    mesh-part membership, support topology, etc.) belongs to the local object
    descriptor library on both server and client. Keeping this DTO runtime-only
    prevents ordinary snapshots from retransmitting deterministic catalog data.
*/
struct ObjectModuleSnapshot
{
    std::string moduleId;
    std::uint8_t state = 0;
    float health = 0.0f;
    int aliveSupportCount = 0;
};

} // namespace game::simulation
