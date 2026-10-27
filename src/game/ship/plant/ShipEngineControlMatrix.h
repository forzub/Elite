#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/equipment/engine/Engine.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"

namespace game::ship::plant
{
// Engine execution report, not a reconstruction from ship acceleration.
// Published alongside authoritative motion for any HUD or diagnostics.
struct EngineMatrixSample
{
    double deliveredThrust01 = 0.0;
    double availableThrust01 = 0.0;
    double nozzleYawRad = 0.0;
    double nozzlePitchRad = 0.0;
    glm::dvec3 forceBodyNewtons {0.0};
    double electricalMW = 0.0;
    double heatMW = 0.0;
};

struct EngineMatrixResult
{
    static constexpr std::size_t MaxEngines = 16;
    bool valid = false;
    std::size_t count = 0;
    glm::dvec3 forceBodyNewtons {0.0};
    glm::dvec3 torqueBodyNm {0.0};
    glm::dvec3 accelerationBodyMps2 {0.0};
    glm::dvec3 angularAccelerationBodyRadPerSec2 {0.0};
    std::array<EngineMatrixSample, MaxEngines> engines {};
};

struct EngineMatrixDemand
{
    glm::dvec3 accelerationBodyMps2 {0.0};
    // pitch, yaw, roll in ShipTransform sign convention
    glm::dvec3 angularAccelerationBodyRadPerSec2 {0.0};
    // No historical servo state or gimbal slew step: nozzle direction
    // is a direct gameplay actuator, solved afresh for each control tick.
    std::uint64_t tick = 0;
};

namespace matrix_detail
{
inline double clampFinite(double v, double lower, double upper) noexcept
{
    return std::isfinite(v) ? std::clamp(v, lower, upper) : lower;
}

// Orthogonal projection onto the exact 3D positive thrust-vector cone.
// q[0] is axial force, q[1:2] transverse force, each in fractions of
// its OWN engine's rated newtons. A nozzle never produces lateral force
// with zero axial thrust.
inline void projectThrustCone(
    std::array<double, 3>& q, double cap01, double maxAngleRad
) noexcept
{
    const double cap = clampFinite(cap01, 0.0, 1.0);
    const double t = std::tan(clampFinite(maxAngleRad, 0.0, 1.45));
    if (cap <= 0.0) { q = {}; return; }
    if (t <= 1.0e-12)
    {
        q[0] = clampFinite(q[0], 0.0, cap);
        q[1] = q[2] = 0.0;
        return;
    }
    double axial = std::isfinite(q[0]) ? q[0] : 0.0;
    double ux = (std::isfinite(q[1]) ? q[1] : 0.0) / t;
    double uy = (std::isfinite(q[2]) ? q[2] : 0.0) / t;
    const double n = std::hypot(ux, uy);
    if (n <= -axial) { q = {}; return; }
    if (n > axial)
    {
        const double c = (n + axial) * 0.5;
        ux *= c / n;
        uy *= c / n;
        axial = c;
    }
    q = {std::max(0.0, axial), ux * t, uy * t};
    const double length = std::sqrt(
        q[0]*q[0] + q[1]*q[1] + q[2]*q[2]
    );
    if (length > cap)
        for (double& v : q) v *= cap / length;
}
} // namespace matrix_detail

// Actual I/Y/X actuator matrix: three force components per installed
// engine and their r x F contributions. No ship-wide synthetic side force.
// Fixed-iteration projected least squares is deterministic and bounded for
// both live flight and tests (<=16 engines; no heap allocations in solver).
[[nodiscard]] inline EngineMatrixResult executeEngineControlMatrix(
    PhysicalControlModelId model,
    const std::vector<game::equipment::engine::MountedEngine>& mounted,
    const ShipParams& ship,
    const EngineMatrixDemand& demand
) noexcept
{
    constexpr std::size_t Max = EngineMatrixResult::MaxEngines;
    EngineMatrixResult out;
    const auto& topology = physicalControlSlots(model);
    if (topology.size() != mounted.size() || topology.size() > Max)
        return out;

    using game::equipment::engine::Engine;
    using game::equipment::engine::EngineCommand;
    struct Motor
    {
        double rated = 0.0;
        double cap = 0.0;
        double limit = 0.0;
        double sign = -1.0;
        glm::dvec3 arm {0.0};
        std::array<std::array<double, 6>, 3> columns {};
        std::array<double, 3> q {};
    };
    std::array<Motor, Max> m {};
    const double mass = std::max(1.0, ship.massKg);
    const glm::dvec3 inertia {
        std::max(1.0, ship.pitchInertiaKgM2),
        std::max(1.0, ship.yawInertiaKgM2),
        std::max(1.0, ship.rollInertiaKgM2)
    };
    glm::dvec3 extents = ship.physicalActuatorHalfExtentsMeters;
    if (extents.x <= 0.0 || extents.y <= 0.0 || extents.z <= 0.0)
        extents = glm::dvec3(std::max(1.0f, ship.turnRadius));

    // Normalize translation and rotation into comparable control budgets.
    double totalForce = 0.0;
    glm::dvec3 rotationalScale {0.1};
    for (std::size_t i=0; i<mounted.size(); ++i)
    {
        if (topology[i].role == EngineSlotRole::ManoeuvreCluster)
            continue; // Newtonian RCS remains a separate actuator family.
        Engine engine(mounted[i]);
        m[i].rated = engine.ratedThrustNewtons();
        if (m[i].rated <= 0.0) continue;
        m[i].cap = std::clamp(
            engine.instantaneousAvailableThrustNewtons(demand.tick) /
                m[i].rated, 0.0, 1.0
        );
        const auto idle = engine.execute({
            0.0, topology[i].nominalForceDirectionBody, demand.tick
        });
        m[i].limit = idle.availableGimbalRangeRad;
        // Mechanical slew is intentionally ignored for this fictional
        // near-instant vectoring engine. One tick directly applies the
        // requested nozzle direction; no last-frame state is integrated.
        // The installed engine passport (and its vectoring damage) owns the
        // nozzle envelope; the flight law never narrows or widens it.
        // The installation topology, not a hard-coded engine kind,
        // supplies the neutral nozzle orientation. All present I/Y/X main
        // mounts are axial (+Z fore / -Z aft).
        m[i].sign =
            topology[i].nominalForceDirectionBody.z >= 0.0 ? 1.0 : -1.0;
        m[i].arm =
            topology[i].normalizedPositionBody * extents -
            ship.centerOfMassLocalMeters;
        totalForce += m[i].rated;
        // Per-engine column maps local force x/y/z into body angular
        // acceleration; roll sign is negative in ShipTransform.
        const std::array<glm::dvec3, 3> axes {
            glm::dvec3 {0.0,0.0,m[i].sign},
            glm::dvec3 {1.0,0.0,0.0},
            glm::dvec3 {0.0,1.0,0.0}
        };
        for (std::size_t j=0;j<3;++j)
        {
            const glm::dvec3 torque =
                glm::cross(m[i].arm, axes[j]*m[i].rated);
            auto& c=m[i].columns[j];
            c[0]=axes[j].x*m[i].rated/mass;
            c[1]=axes[j].y*m[i].rated/mass;
            c[2]=axes[j].z*m[i].rated/mass;
            c[3]=torque.x/inertia.x;
            c[4]=torque.y/inertia.y;
            c[5]=-torque.z/inertia.z;
            const double slope = j == 0 ? 1.0 :
                std::tan(std::clamp(m[i].limit,0.0,1.45));
            rotationalScale.x += std::abs(c[3])*slope;
            rotationalScale.y += std::abs(c[4])*slope;
            rotationalScale.z += std::abs(c[5])*slope;
        }
    }
    // Neutral control: skip the solver, immediately return every
    // nozzle to neutral without inventing a post-release servo transient.
    if (glm::length(demand.accelerationBodyMps2)<1.0e-12 &&
        glm::length(demand.angularAccelerationBodyRadPerSec2)<1.0e-12)
    {
        out.valid=true;
        out.count=mounted.size();
        for(std::size_t i=0;i<mounted.size();++i)
        {
            auto& t=out.engines[i];
            const game::equipment::engine::Engine installed(mounted[i]);
            t.availableThrust01=m[i].rated>0.0
                ? installed.availableThrustNewtons()/m[i].rated
                : 0.0;
            t.nozzleYawRad = 0.0;
            t.nozzlePitchRad = 0.0;
        }
        return out;
    }

    // Main-axis only is a PREDEFINED actuator maneuver, not a 6-DOF
    // optimization problem. Num8/Num2 and straight-line thrust must never
    // turn a nozzle or introduce artificial pitch/yaw/roll torque on a
    // healthy symmetric I/Y/X layout. Distribute an equal actual Newton
    // force over surviving engines of the requested bank. This also keeps
    // damaged-bank balancing deterministic; absent engines can still leave
    // a physically unavoidable torque, which we do not hide.
    const double axialDemand = demand.accelerationBodyMps2.z;
    const double negligibleCrossAxis =
        std::max(1.0e-6, std::abs(axialDemand)*1.0e-6);
    const bool mainAxisOnly =
        std::abs(axialDemand) > 1.0e-9 &&
        std::abs(demand.accelerationBodyMps2.x) <= negligibleCrossAxis &&
        std::abs(demand.accelerationBodyMps2.y) <= negligibleCrossAxis &&
        glm::length(demand.angularAccelerationBodyRadPerSec2) < 1.0e-9;
    if (mainAxisOnly)
    {
        const auto bank = axialDemand < 0.0
            ? EngineSlotRole::AftMain : EngineSlotRole::ForeMain;
        std::size_t surviving = 0;
        double minimumAvailableN = std::numeric_limits<double>::infinity();
        for (std::size_t i=0;i<mounted.size();++i)
        {
            if (topology[i].role != bank || m[i].cap <= 0.0)
                continue;
            ++surviving;
            minimumAvailableN = std::min(
                minimumAvailableN, m[i].rated*m[i].cap
            );
        }
        out.valid = true;
        out.count = mounted.size();
        const double equalDeliveredN = surviving > 0
            ? std::min(
                std::abs(axialDemand)*mass/static_cast<double>(surviving),
                minimumAvailableN
            ) : 0.0;
        for (std::size_t i=0;i<mounted.size();++i)
        {
            auto& sample = out.engines[i];
            const Engine engine(mounted[i]);
            sample.availableThrust01 = m[i].rated > 0.0
                ? engine.availableThrustNewtons()/m[i].rated : 0.0;
            if (topology[i].role != bank || m[i].cap <= 0.0)
                continue;
            const double instantaneousN = m[i].rated*m[i].cap;
            const auto applied = engine.execute(EngineCommand {
                equalDeliveredN / instantaneousN,
                topology[i].nominalForceDirectionBody,
                demand.tick
            });
            sample.deliveredThrust01 = applied.deliveredNominalThrust01;
            sample.forceBodyNewtons = applied.forceBodyNewtons;
            sample.electricalMW = applied.consumedElectricalMW;
            sample.heatMW = applied.generatedWasteHeatMW;
            out.forceBodyNewtons += applied.forceBodyNewtons;
            out.torqueBodyNm +=
                glm::cross(m[i].arm, applied.forceBodyNewtons);
        }
        out.accelerationBodyMps2 = out.forceBodyNewtons/mass;
        out.angularAccelerationBodyRadPerSec2 = {
            out.torqueBodyNm.x/inertia.x,
            out.torqueBodyNm.y/inertia.y,
            -out.torqueBodyNm.z/inertia.z
        };
        return out;
    }

    // Ordinary purely longitudinal acceleration must never ignite the
    // opposite bank just to improve an optimizer norm by a fraction.
    // For side forces and body turns both directions remain available.
    const bool pureAxial =
        std::abs(demand.accelerationBodyMps2.x)<1.0e-9 &&
        std::abs(demand.accelerationBodyMps2.y)<1.0e-9 &&
        glm::length(demand.angularAccelerationBodyRadPerSec2)<1.0e-9;
    if (pureAxial)
        for(std::size_t i=0;i<mounted.size();++i)
            if ((demand.accelerationBodyMps2.z < -1.0e-9 &&
                 topology[i].role == EngineSlotRole::ForeMain) ||
                (demand.accelerationBodyMps2.z > 1.0e-9 &&
                 topology[i].role == EngineSlotRole::AftMain))
                m[i].cap=0.0;

    const double linearScale = std::max(1.0, totalForce / mass);
    const std::array<double,6> scales {
        linearScale,linearScale,linearScale,
        std::max(0.2,rotationalScale.x),
        std::max(0.2,rotationalScale.y),
        std::max(0.2,rotationalScale.z)
    };
    const std::array<double,6> target {
        demand.accelerationBodyMps2.x/scales[0],
        demand.accelerationBodyMps2.y/scales[1],
        demand.accelerationBodyMps2.z/scales[2],
        demand.angularAccelerationBodyRadPerSec2.x/scales[3],
        demand.angularAccelerationBodyRadPerSec2.y/scales[4],
        demand.angularAccelerationBodyRadPerSec2.z/scales[5]
    };
    // Priority: arrest hull angular velocity EVEN WHILE the fore/aft bank
    // is executing a manual keypad impulse or subsequent assisted braking.
    // The old 3:1 weight preserved axial force but delivered only ~18% of
    // demanded pitch braking at nominal axial thrust, allowing uncontrolled
    // pitch buildup and controller resonance after Num8/Num2 release.
    // A dimensionless 80:1 angular-error weight keeps all I/Y/X layouts
    // within the accepted rotational response while still using actual
    // engine forces and moments (no fake attitude delta).
    const std::array<double,6> weights {1.0,1.0,1.0,80.0,80.0,80.0};
    for (auto& motor:m)
        for(auto& col:motor.columns)
            for(std::size_t k=0;k<6;++k) col[k]/=scales[k];

    // Convex six-axis objective, projected on each engine's physical cone.
    // FISTA uses a GLOBAL conservative Lipschitz step, so cross-coupled
    // engine torques cannot drive the old cyclic-coordinate overshoot into
    // equal 100% opposed burns. This is deterministic and matrix-size bounded.
    constexpr double Regularization=0.001;
    std::array<std::array<double,3>,Max> velocity {};
    std::array<std::array<double,3>,Max> next {};
    double lipschitz=Regularization;
    for (std::size_t i=0;i<mounted.size();++i)
        for (std::size_t j=0;j<3;++j)
            for (std::size_t k=0;k<6;++k)
                lipschitz += weights[k]*m[i].columns[j][k]*
                             m[i].columns[j][k];
    // Warm start from the exact equal-bank axial maneuver used above for
    // pure main-axis demand. One global FISTA step is dominated by the large
    // gimbal-torque columns, so starting from zero left the axial component
    // at ~14% after the fixed pass budget whenever any small lateral demand
    // was present. The solver now only refines lateral/torque terms.
    {
        const double axial = demand.accelerationBodyMps2.z;
        const auto bank = axial < 0.0
            ? EngineSlotRole::AftMain : EngineSlotRole::ForeMain;
        std::size_t surviving = 0;
        for (std::size_t i=0;i<mounted.size();++i)
            if (topology[i].role == bank && m[i].rated > 0.0 &&
                m[i].cap > 0.0)
                ++surviving;
        for (std::size_t i=0;i<mounted.size() && surviving>0;++i)
        {
            if (topology[i].role != bank || m[i].rated <= 0.0 ||
                m[i].cap <= 0.0)
                continue;
            const double fraction = std::min(
                m[i].cap,
                std::abs(axial)*mass/
                    (static_cast<double>(surviving)*m[i].rated)
            );
            m[i].q = {fraction, 0.0, 0.0};
            velocity[i] = m[i].q;
        }
    }
    double momentum=1.0;
    // 96 rounds suffice for the nominal Cobra 6-DOF envelope without
    // simulating nozzle kinematics; avoids expensive 240-pass NPC loops.
    for (int pass=0;pass<96;++pass)
    {
        std::array<double,6> residual {};
        for (std::size_t k=0;k<6;++k) residual[k]=-target[k];
        for (std::size_t i=0;i<mounted.size();++i)
            for (std::size_t j=0;j<3;++j)
                for (std::size_t k=0;k<6;++k)
                    residual[k]+=m[i].columns[j][k]*velocity[i][j];

        for (std::size_t i=0;i<mounted.size();++i)
        {
            next[i]=velocity[i];
            for (std::size_t j=0;j<3;++j)
            {
                double gradient=Regularization*velocity[i][j];
                for (std::size_t k=0;k<6;++k)
                    gradient += weights[k]*m[i].columns[j][k]*residual[k];
                next[i][j] -= gradient/lipschitz;
            }
            matrix_detail::projectThrustCone(
                next[i],m[i].cap,m[i].limit
            );
        }

        const double newMomentum=
            0.5*(1.0+std::sqrt(1.0+4.0*momentum*momentum));
        const double extrapolation=(momentum-1.0)/newMomentum;
        for (std::size_t i=0;i<mounted.size();++i)
            for(std::size_t j=0;j<3;++j)
            {
                const double old=m[i].q[j];
                m[i].q[j]=next[i][j];
                velocity[i][j]=m[i].q[j]+
                    extrapolation*(m[i].q[j]-old);
            }
        momentum=newMomentum;
    }

    out.count=mounted.size();
    out.valid=true;
    for (std::size_t i=0;i<mounted.size();++i)
    {
        if (m[i].rated<=0.0) continue;
        const auto& motor=m[i];
        auto& sample=out.engines[i];
        const double forceLen=std::sqrt(
            motor.q[0]*motor.q[0]+motor.q[1]*motor.q[1]+
            motor.q[2]*motor.q[2]
        );
        double yaw=std::atan2(motor.q[1],std::max(1.0e-12,motor.q[0]));
        double pitch=std::atan2(motor.q[2],std::max(1.0e-12,motor.q[0]));
        // Aim immediately at the solved direction, clamped ONLY by
        // this engine's available nozzle cone. There is no simulated
        // motor latency, previous-frame angle, or hidden second slew limit.
        const double combined=std::hypot(yaw,pitch);
        if (combined>motor.limit && combined>0.0)
        {
            yaw*=motor.limit/combined;
            pitch*=motor.limit/combined;
        }
        const glm::dvec3 direction=glm::normalize(glm::dvec3(
            std::tan(yaw),std::tan(pitch),motor.sign
        ));
        const Engine output=Engine(mounted[i]);
        auto applied=output.execute(EngineCommand {
            forceLen / std::max(1.0e-12,motor.cap),
            direction,demand.tick
        });
        sample.deliveredThrust01=applied.deliveredNominalThrust01;
        // Yellow reports sustained damaged/thermal capability, not a
        // stochastic instantaneous hiccup (only RED may fluctuate).
        sample.availableThrust01=motor.rated>0.0
            ? output.availableThrustNewtons()/motor.rated
            : 0.0;
        sample.nozzleYawRad=yaw;
        sample.nozzlePitchRad=pitch;
        sample.forceBodyNewtons=applied.forceBodyNewtons;
        sample.electricalMW=applied.consumedElectricalMW;
        sample.heatMW=applied.generatedWasteHeatMW;
        out.forceBodyNewtons+=sample.forceBodyNewtons;
        out.torqueBodyNm+=glm::cross(motor.arm,sample.forceBodyNewtons);
    }
    out.accelerationBodyMps2=out.forceBodyNewtons/mass;
    out.angularAccelerationBodyRadPerSec2={
        out.torqueBodyNm.x/inertia.x,
        out.torqueBodyNm.y/inertia.y,
        -out.torqueBodyNm.z/inertia.z
    };
    return out;
}
} // namespace game::ship::plant
