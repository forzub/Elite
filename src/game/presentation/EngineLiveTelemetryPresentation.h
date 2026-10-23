#pragma once

#include <algorithm>
#include <cstddef>

#include "src/game/navigation/DynamicMotionState.h"

namespace game::presentation
{
// Independent live-operation instrumentation, separate from fault-oriented
// BITE. The UI reads executed actuator measurements only, NEVER infers
// per-engine throttle from ship acceleration or pilot controls.
struct EngineMotorLiveView
{
    bool sampled = false;
    double thrust01 = 0.0;
    double available01 = 0.0;
    double nozzleYawRad = 0.0;
    double nozzlePitchRad = 0.0;
    glm::dvec3 forceBodyNewtons {0.0};
    double electricalMW = 0.0;
    double wasteHeatMW = 0.0;
    bool hasDetailedActuatorSample = false;
};

[[nodiscard]] inline EngineMotorLiveView readEngineMotorLiveView(
    const game::navigation::DynamicMotionState& motion,
    std::size_t index,
    double projectedCapability01
) noexcept
{
    EngineMotorLiveView out;
    out.available01 = std::clamp(projectedCapability01,0.0,1.0);
    const auto& t=motion.engineTelemetry;
    if (t.valid && index<t.count)
    {
        out.sampled=true;
        out.hasDetailedActuatorSample=true;
        out.thrust01=std::clamp(t.thrust01[index],0.0,1.0);
        // An executed-actuator sample can lag the module-damage snapshot.
        // Never let an older healthy sample resurrect a failed motor on the
        // HUD. The physical damage projection is a hard availability ceiling.
        out.available01=std::min(out.available01,
            std::clamp(t.available01[index],0.0,1.0));
        // A stale executed-thrust sample cannot extend beyond current
        // installed hardware capability (including a latched failure).
        out.thrust01=std::min(out.thrust01,out.available01);
        out.nozzleYawRad=t.nozzleYawRad[index];
        out.nozzlePitchRad=t.nozzlePitchRad[index];
        out.forceBodyNewtons=t.forceBodyNewtons[index];
        out.electricalMW=t.electricalMW[index];
        out.wasteHeatMW=t.wasteHeatMW[index];
    }
    else if (index<motion.mainEngineReportedSlotCount)
    {
        // Newtonian legacy executor already reports the actual physical
        // delivered fraction; do not invent angular loads or nozzle pose.
        out.sampled=true;
        out.thrust01=std::clamp(
            motion.mainEngineDeliveredThrust01[index],0.0,1.0
        );
    }
    return out;
}
} // namespace game::presentation
