#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace game::simulation
{

// Replicated read-only truth for the exact Navigation v2 command product that
// authoritative GameSimulation used for this ship. Acceleration vectors are\n// expressed in the authoritative system/world axes consumed by physics.
//
// This is diagnostic/guidance state only. Clients may display it but must not
// feed it back into planning or physics.
struct NavigationExecutionSnapshot
{
    bool valid = false;

    std::uint64_t intentRevision = 0;
    std::uint64_t activeTargetRevision = 0;

    glm::dvec3 idealLinearAccelerationDemandSystemMps2 {0.0};
    glm::dvec3 idealAngularAccelerationDemandSystemRadPerSec2 {0.0};

    glm::dvec3 executedLinearAccelerationDemandSystemMps2 {0.0};
    glm::dvec3 executedAngularAccelerationDemandSystemRadPerSec2 {0.0};

    bool emergency = false;
    double hazardUrgency01 = 0.0;

    bool reactionBlocked = false;
    bool decisionSampled = false;
    bool queuedCommandApplied = false;
    std::uint32_t pendingCommandCount = 0;
};

} // namespace game::simulation
