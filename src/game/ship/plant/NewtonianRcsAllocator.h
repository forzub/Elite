#pragma once

// Deterministic fixed-direction Newtonian RCS wrench allocator.
// No heap allocation in the solver; all 24 nozzle force columns are fixed.
// This never creates force outside an installed and available engine and
// never assigns a fictional gimbal direction to an RCS nozzle.
// One 6D demand -> actual force + moment + per-nozzle usage and electrical/
// thermal measurements. The authoritative server must supply 24 real
// MountedEngine views from its ObjectModuleRuntime at the control tick.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

#include "src/game/equipment/engine/Engine.h"
#include "src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h"

namespace game::ship::plant
{
struct NewtonianRcsDemand
{
    glm::dvec3 forceBodyN {0.0};
    glm::dvec3 momentBodyNm {0.0};
    // Physical half-extents from reactor/fuel CoM in metres, NOT normalized.
    glm::dvec3 mountHalfExtentsM {1.0};
    std::uint64_t tick = 0;
};

struct NewtonianRcsResult
{
    bool valid = false;
    glm::dvec3 deliveredForceN {0.0};
    glm::dvec3 deliveredMomentNm {0.0};
    std::array<double, 24> throttle01 {};
    std::array<double, 24> availabilityN {};
    std::array<double, 24> electricalMW {};
    std::array<double, 24> wasteHeatMW {};
};

// A bounded deterministic coordinate-descent least-squares solution.
// Force and torque are normalized by independently estimated full-bank
// capacities so neither physical unit dominates due only to scale.
// The returned physical wrench, not the request, is the only executable
// result. A single damaged nozzle simply reduces its own force column.
[[nodiscard]] inline NewtonianRcsResult allocateNewtonianRcs(
    const std::array<game::equipment::engine::MountedEngine, 24>& installed,
    const NewtonianRcsDemand& demand
) noexcept
{
    NewtonianRcsResult out;
    const auto& mounts =
        game::ship::presets::cobra::newtonianRcsNozzles();
    std::array<glm::dvec3,24> forces {};
    std::array<glm::dvec3,24> moments {};
    std::array<double,24> cap {};

    double forceScale=0.0;
    double momentScale=0.0;
    for (std::size_t i=0; i<24; ++i)
    {
        const game::equipment::engine::Engine engine(installed[i]);
        cap[i]=std::max(0.0,
            engine.instantaneousAvailableThrustNewtons(demand.tick));
        out.availabilityN[i]=cap[i];
        const auto& mount=mounts[i];
        const glm::dvec3 r=mount.positionBody *
            demand.mountHalfExtentsM;
        forces[i]=mount.forceDirectionBody*cap[i];
        moments[i]=glm::cross(r,forces[i]);
        forceScale+=cap[i];
        momentScale+=glm::length(moments[i]);
    }
    forceScale=std::max(1.0,forceScale);
    momentScale=std::max(1.0,momentScale);
    std::array<glm::dvec3,24> f {};
    std::array<glm::dvec3,24> m {};
    for(std::size_t i=0;i<24;++i)
    {
        f[i]=forces[i]/forceScale;
        m[i]=moments[i]/momentScale;
    }
    const glm::dvec3 wantF=demand.forceBodyN/forceScale;
    const glm::dvec3 wantM=demand.momentBodyNm/momentScale;
    glm::dvec3 haveF {0.0},haveM {0.0};
    // Fixed iterations: predictable server cost, no dynamic optimizer.
    for(int iteration=0;iteration<32;++iteration)
    {
        for(std::size_t i=0;i<24;++i)
        {
            const double norm=glm::dot(f[i],f[i])+
                glm::dot(m[i],m[i]);
            if(norm<=1e-20) continue;
            const double gradient=
                glm::dot(f[i],wantF-haveF)+
                glm::dot(m[i],wantM-haveM);
            const double before=out.throttle01[i];
            const double after=std::clamp(before+gradient/norm,
                                         0.0,1.0);
            out.throttle01[i]=after;
            haveF+=(after-before)*f[i];
            haveM+=(after-before)*m[i];
        }
    }
    for(std::size_t i=0;i<24;++i)
    {
        game::equipment::engine::Engine actuator(installed[i]);
        const game::equipment::engine::EngineCommand command {
            out.throttle01[i],mounts[i].forceDirectionBody,demand.tick
        };
        const auto sample=actuator.execute(command);
        out.deliveredForceN+=sample.forceBodyNewtons;
        const glm::dvec3 r=mounts[i].positionBody *
            demand.mountHalfExtentsM;
        out.deliveredMomentNm+=glm::cross(r,sample.forceBodyNewtons);
        out.electricalMW[i]=sample.consumedElectricalMW;
        out.wasteHeatMW[i]=sample.generatedWasteHeatMW;
    }
    out.valid=true;
    return out;
}
} // namespace game::ship::plant
