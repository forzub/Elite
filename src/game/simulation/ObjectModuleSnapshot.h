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

    // Runtime equipment damage causes. Sensitivity weights remain static in
    // the local descriptor and are not retransmitted.
    float laserDamage01 = 0.0f;
    float impactDamage01 = 0.0f;
    float thermalDamage01 = 0.0f;
    float radiationDamage01 = 0.0f;
    float wearDamage01 = 0.0f;

    // Per-installed-engine state needed for IDENTICAL server/client physics
    // capability projection. Static thermal constants/material IDs are held
    // in both sides' immutable ship descriptor, not sent over the network.
    // false maintains backward compatibility with old or non-engine modules.
    bool hasEngineThermalState = false;
    float engineTemperatureK = 0.0f;
    float engineCoolantTemperatureK = 0.0f;
    float engineImpactShock01 = 0.0f;
    float engineImpactShockSecondsRemaining = 0.0f;
    float emitterIntactArea01 = 1.0f;
    float emitterMicrocrack01 = 0.0f;
    float emitterLaserDamage01 = 0.0f;
    float emitterImpactDamage01 = 0.0f;
    float emitterThermalDamage01 = 0.0f;
    float emitterRadiationDamage01 = 0.0f;
    float emitterWearDamage01 = 0.0f;
    float emitterEquivalentOperatingHours = 0.0f;

    int aliveSupportCount = 0;
};

} // namespace game::simulation
