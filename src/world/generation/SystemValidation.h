#pragma once

#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

// Independent physical gates over a finished system descriptor. Appends one
// StabilityCheck per gate family (failed gates carry the offending bodies)
// and returns true when every gate passed.
bool validateSystem(SystemDescriptor& system);

} // namespace world::generation
