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
#include <limits>
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
    // Installed nozzles produce torque around the live cargo/fuel centre of mass.
    glm::dvec3 centerOfMassLocalMeters {0.0};
    // Electrical MW remaining on the dedicated ship-services bus this tick.
    // Infinity is reserved for isolated unit tests that do not model a bus.
    double availableElectricalMW = std::numeric_limits<double>::infinity();
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

// Resource gameplay policy for the actual installed 24-nozzle plant.
// The gas is a replenished manoeuvre budget, NOT the fuel/propellant physics.
// It constrains commands BEFORE they become actual physical force/torque;
// never revoke a delivered impulse after a nozzle has fired.
[[nodiscard]] inline double advanceNewtonianRcsGas(
    double& pressure01, bool& depleted, double dt,
    double requestedIntensity01, double usePerSecond,
    double rechargePerSecond, double restartFraction
) noexcept
{
    if (!std::isfinite(dt) || dt<=0.0) return 0.0;
    pressure01=std::clamp(
        std::isfinite(pressure01)?pressure01:0.0,0.0,1.0);
    const double charge=std::max(0.0,rechargePerSecond)*dt;
    const double restart=std::clamp(restartFraction,0.0,1.0);
    if(depleted)
    {
        // Nozzles are shut: the compressor refills the manifold.
        pressure01=std::min(1.0,pressure01+charge);
        if(pressure01>=restart) depleted=false;
        return 0.0; // Restart next tick; this tick's nozzles stay off.
    }
    const double requested=std::clamp(requestedIntensity01,0.0,1.0);
    // The manifold only refills while the nozzles draw nothing: a firing
    // RCS consumes its stored gas, recovery starts when consumption stops
    // (the gas, not the recharge rate, bounds what the RCS can deliver).
    const double available=requested>0.0
        ? pressure01 : std::min(1.0,pressure01+charge);
    const double cost=std::max(0.0,usePerSecond)*requested*dt;
    const double factor=cost>1.0e-12
        ? std::clamp(available/cost,0.0,1.0):1.0;
    pressure01=std::max(0.0,available-factor*cost);
    if(cost>1.0e-12 && pressure01<=1.0e-12)
        depleted=true;
    return factor;
}

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
            demand.mountHalfExtentsM - demand.centerOfMassLocalMeters;
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
    // Enforce available bus power BEFORE committing forces. Engine::execute
    // is a deterministic sample of an immutable module, not a state update.
    // A common duty scale preserves the allocator's force/torque balance.
    double requestedMW=0.0;
    for(std::size_t i=0;i<24;++i)
    {
        const game::equipment::engine::Engine actuator(installed[i]);
        const game::equipment::engine::EngineCommand command {
            out.throttle01[i],mounts[i].forceDirectionBody,demand.tick
        };
        requestedMW+=std::max(0.0,actuator.execute(command).consumedElectricalMW);
    }
    double electricalScale=1.0;
    if(std::isfinite(demand.availableElectricalMW) && requestedMW>1.0e-12)
        electricalScale=std::clamp(
            std::max(0.0,demand.availableElectricalMW)/requestedMW,0.0,1.0);
    for(std::size_t i=0;i<24;++i)
    {
        out.throttle01[i]*=electricalScale;
        const game::equipment::engine::Engine actuator(installed[i]);
        const game::equipment::engine::EngineCommand command {
            out.throttle01[i],mounts[i].forceDirectionBody,demand.tick
        };
        const auto sample=actuator.execute(command);
        out.deliveredForceN+=sample.forceBodyNewtons;
        const glm::dvec3 r=mounts[i].positionBody *
            demand.mountHalfExtentsM - demand.centerOfMassLocalMeters;
        out.deliveredMomentNm+=glm::cross(r,sample.forceBodyNewtons);
        out.electricalMW[i]=sample.consumedElectricalMW;
        out.wasteHeatMW[i]=sample.generatedWasteHeatMW;
    }
    out.valid=true;
    return out;
}
// Feasible independent signed wrench limits of the SAME 24 installed
// damageable nozzles. This is a CONTROL/PLANNING projection, not extra
// thrust: every bound is sampled through the actual physical allocator.
// Neutral zero moment/force is requested on the other five axes. Resource
// pressure/power are applied separately before each actual execution.
struct NewtonianRcsSignedAuthority
{
    glm::dvec3 positiveForceN {0.0};
    glm::dvec3 negativeForceN {0.0};
    glm::dvec3 positiveMomentNm {0.0};
    glm::dvec3 negativeMomentNm {0.0};
};

[[nodiscard]] inline NewtonianRcsSignedAuthority
sampleNewtonianRcsSignedAuthority(
    const std::array<game::equipment::engine::MountedEngine,24>& installed,
    const glm::dvec3& halfExtentsMeters,
    const glm::dvec3& centerOfMassLocalMeters
) noexcept
{
    NewtonianRcsSignedAuthority authority;
    const auto& mounts=game::ship::presets::cobra::newtonianRcsNozzles();
    double totalForceN=0.0,totalMomentNm=0.0;
    for(std::size_t i=0;i<installed.size();++i)
    {
        const game::equipment::engine::Engine engine(installed[i]);
        const double capacity=std::max(
            0.0,engine.instantaneousAvailableThrustNewtons(0));
        totalForceN+=capacity;
        const glm::dvec3 lever=
            mounts[i].positionBody*halfExtentsMeters-
            centerOfMassLocalMeters;
        totalMomentNm+=glm::length(glm::cross(
            lever,mounts[i].forceDirectionBody*capacity));
    }
    if(totalForceN<=1.0e-9)return authority;

    for(int axis=0;axis<6;++axis)
        for(int sign=-1;sign<=1;sign+=2)
        {
            NewtonianRcsDemand probe;
            probe.mountHalfExtentsM=halfExtentsMeters;
            probe.centerOfMassLocalMeters=centerOfMassLocalMeters;
            probe.tick=0;
            // 24 nozzles cannot generate a force/moment greater than
            // their entire individual force/moment magnitude sums.
            // Request that mathematical ceiling, without inventing 2m/s2
            // or 3rad/s2 when the real Cobra cannot actually achieve it.
            if(axis<3)
                probe.forceBodyN[axis]=sign*totalForceN;
            else
                probe.momentBodyNm[axis-3]=sign*totalMomentNm;
            const auto sample=allocateNewtonianRcs(installed,probe);
            if(!sample.valid)continue;
            if(axis<3)
            {
                const double delivered=std::max(
                    0.0,sign*sample.deliveredForceN[axis]);
                (sign>0 ? authority.positiveForceN :
                    authority.negativeForceN)[axis]=delivered;
            }
            else
            {
                const double delivered=std::max(
                    0.0,sign*sample.deliveredMomentNm[axis-3]);
                (sign>0 ? authority.positiveMomentNm :
                    authority.negativeMomentNm)[axis-3]=delivered;
            }
        }
    return authority;
}


// Combined-demand feasibility of the SAME 24 nozzles: for every pair of
// signed axes (force / moment) both are requested at once at their own
// single-axis limits; the result is the smallest delivered fraction of a
// requested component over all pairs (1 = the per-axis limits can be used
// together, < 1 = the axes share nozzles and cannot both be at full).
[[nodiscard]] inline double newtonianRcsPairFeasibility01(
    const std::array<game::equipment::engine::MountedEngine,24>& installed,
    const glm::dvec3& halfExtentsMeters,
    const glm::dvec3& centerOfMassLocalMeters,
    const NewtonianRcsSignedAuthority& limits) noexcept
{
    const auto limit=[&](int axis,int sign){
        return axis<3
            ? (sign>0 ? limits.positiveForceN : limits.negativeForceN)[axis]
            : (sign>0 ? limits.positiveMomentNm : limits.negativeMomentNm)[axis-3];
    };
    double worst=1.0;
    for(int a=0;a<6;++a)
        for(int b=a+1;b<6;++b)
            for(int sa=-1;sa<=1;sa+=2)
                for(int sb=-1;sb<=1;sb+=2)
                {
                    const double la=limit(a,sa), lb=limit(b,sb);
                    if(la<=1.0e-9 || lb<=1.0e-9)
                        continue;
                    NewtonianRcsDemand probe;
                    probe.mountHalfExtentsM=halfExtentsMeters;
                    probe.centerOfMassLocalMeters=centerOfMassLocalMeters;
                    probe.tick=0;
                    const auto set=[&](int axis,int sign,double value){
                        if(axis<3) probe.forceBodyN[axis]=sign*value;
                        else probe.momentBodyNm[axis-3]=sign*value;
                    };
                    set(a,sa,la);
                    set(b,sb,lb);
                    const auto sample=allocateNewtonianRcs(installed,probe);
                    if(!sample.valid)
                        return 0.0;
                    const auto delivered=[&](int axis,int sign){
                        return axis<3 ? sign*sample.deliveredForceN[axis]
                                      : sign*sample.deliveredMomentNm[axis-3];
                    };
                    worst=std::min({worst,
                        std::max(0.0,delivered(a,sa))/la,
                        std::max(0.0,delivered(b,sb))/lb});
                }
    return std::clamp(worst,0.0,1.0);
}

} // namespace game::ship::plant
