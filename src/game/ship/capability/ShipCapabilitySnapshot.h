#pragma once

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::capability
{

enum class ControlHealthState : std::uint8_t
{
    Normal = 0,
    Degraded,
    Survival,
    Uncontrollable
};

struct AxisCapability
{
    double positive = 0.0;
    double negative = 0.0;
};

struct ShipCapabilitySnapshot
{
    bool valid = false;
    ControlHealthState controlState =
        ControlHealthState::Uncontrollable;

    AxisCapability longitudinalAccelerationMps2;
    AxisCapability strafeAccelerationMps2;
    AxisCapability liftAccelerationMps2;

    AxisCapability pitchAccelerationRadPerSec2;
    AxisCapability yawAccelerationRadPerSec2;
    AxisCapability rollAccelerationRadPerSec2;

    AxisCapability pitchRateRadPerSec;
    AxisCapability yawRateRadPerSec;
    AxisCapability rollRateRadPerSec;

    // Transitional bridge for the accepted motion backend. Control, planner and
    // prediction consume capability; this field lets the existing execution
    // backend keep its current ShipParams API while physical topology becomes
    // the source of truth.
    ShipParams effectivePhysics {};
};

} // namespace game::ship::capability
