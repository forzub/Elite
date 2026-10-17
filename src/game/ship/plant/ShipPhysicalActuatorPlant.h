#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"

namespace game::ship::plant
{

// Deterministic instantaneous thrust after equipment-authored instability.
[[nodiscard]] double instantaneousSlotThrust01(
    const EngineSlotState& state,
    std::size_t slotIndex,
    std::uint64_t controlTick
) noexcept;

// Scale applied to the net main acceleration for the currently active bank.
// Healthy/stable slots return exactly 1.0.
[[nodiscard]] double evaluateMainPropulsionInstantaneousScale(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick
) noexcept;

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

struct MainPropulsionTrimResult
{
    bool valid = false;

    // Actual main-engine acceleration after instantaneous damage, throttle
    // balancing and the small automatic common-TVC correction.
    glm::dvec3 accelerationBodyMps2 {0.0};

    MainPropulsionReaction reaction;

    // Diagnostics for HUD/logging. 1 = no thrust sacrificed by balancing.
    double thrustScale01 = 1.0;
    double trimLoad01 = 0.0;
    double automaticYawTrimRad = 0.0;
    double automaticPitchTrimRad = 0.0;
};

// Cheap closed-loop damaged-main compensation.
//
// The ordinary flight law still decides the requested maneuver. This helper
// only reduces individual engines that create harmful parasitic torque and,
// if useful, adds a small common TVC correction. It never increases any engine
// above its current damage-limited/instantaneous authority and never engages
// the opposite main bank.
[[nodiscard]] MainPropulsionTrimResult evaluateMainPropulsionFeedbackTrim(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& requestedAccelerationBodyMps2,
    const glm::dvec3& attitudeIntent01,
    double remainingYawTrimRad,
    double remainingPitchTrimRad,
    std::uint64_t controlTick
) noexcept;

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
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick = 0
) noexcept;

} // namespace game::ship::plant
