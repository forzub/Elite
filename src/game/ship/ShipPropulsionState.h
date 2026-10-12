#pragma once

#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/simulation/ObjectModuleSnapshot.h"
#include "src/world/modules/ObjectModuleRuntime.h"

namespace game::ship
{

// Compatibility projection used by the accepted motion backend.
//
// Runtime propulsion truth now flows through the fixed I/Y/X physical model and
// ShipCapabilitySnapshot. Callers that still consume ShipParams receive only
// the capability projection; they no longer interpret fore/aft engine banks or
// module damage themselves.
inline ShipParams effectiveShipPhysics(
    const ShipDescriptor& descriptor,
    const world::modules::ObjectModuleRuntime& runtime
)
{
    const auto capability =
        game::ship::capability::buildShipCapability(
            descriptor,
            runtime
        );

    return capability.valid
        ? capability.effectivePhysics
        : ShipParams {};
}

// Client/prediction equivalent from replicated module state. Sparse startup
// snapshots deliberately retain healthy descriptor capability until the graph
// arrives, matching the capability builder contract.
inline ShipParams effectiveShipPhysics(
    const ShipDescriptor& descriptor,
    const std::vector<game::simulation::ObjectModuleSnapshot>& modules
)
{
    const auto capability =
        game::ship::capability::buildShipCapability(
            descriptor,
            modules
        );

    return capability.valid
        ? capability.effectivePhysics
        : ShipParams {};
}

} // namespace game::ship
