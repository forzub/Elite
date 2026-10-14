#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"

namespace game::ship::plant
{

struct MainPropulsionReaction
{
    bool valid = false;

    // Sum of r x F for the currently firing longitudinal main bank, expressed
    // in canonical ship-body axes (+X right, +Y up, +Z aft).
    glm::dvec3 torqueBodyNm {0.0};

    // Principal-axis angular acceleration derived from the real torque and
    // current mass properties. Z is converted to ShipTransform roll sign,
    // whose positive axis is ship forward (-body Z).
    glm::dvec3 angularAccelerationBodyRadPerSec2 {0.0};
};

// Convert the already-authorized net main-engine acceleration back into the
// forces carried by the surviving physical slots and calculate their reaction
// torque about the current center of mass.
//
// The accepted motion backend still determines the commanded net main force.
// This function adds the rigid-body consequence that was previously lost when
// per-slot topology was projected into aggregate ShipParams.
//
// normalizedPositionBody is deliberately dimensionless. Production capability
// projection maps it through the ship's real logical half-extents; turnRadius
// remains only a legacy fallback for descriptors/tests that lack geometry.
[[nodiscard]] MainPropulsionReaction evaluateMainPropulsionReaction(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& mainAccelerationBodyMps2
) noexcept;

} // namespace game::ship::plant
