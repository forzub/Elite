#pragma once

// Fixed signed maneuvers for Assisted I/Y/X. The expensive constrained solve
// happens only when geometry or installed motor capability changes. Each flight
// tick is a linear combination of up to six precomputed per-motor vectors:
// no frame-by-frame general optimization and no artificial chassis force.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "src/game/ship/plant/ShipEngineControlMatrix.h"

namespace game::ship::plant
{
struct AssistedManeuverPrograms final
{
    static constexpr std::size_t Max = EngineMatrixResult::MaxEngines;
    static constexpr std::size_t Variables = 3 * Max;
    static constexpr std::size_t Axes = 6;
    struct Motor
    {
        double ratedN = 0.0;
        double capacity01 = 0.0;
        double coneRad = 0.0;
        double axisSign = -1.0;
        glm::dvec3 arm {0.0};
    };
    struct Maneuver
    {
        std::array<double, Variables> forceFractions {};
        double achievable = 0.0; // physical acceleration for this axis
    };

    bool valid = false;
    PhysicalControlModelId model = PhysicalControlModelId::AssistedI;
    std::size_t count = 0;
    ShipParams shape {};
    std::array<Motor, Max> motors {};
    std::array<double, Axes> scales {};
    std::array<std::array<double, Variables>, Axes> wrench {};
    std::array<std::array<double, Axes>, Variables> inverse {};
    std::array<Maneuver, 2 * Axes> programs {};

    [[nodiscard]] static bool almost(double a, double b) noexcept
    {
        return std::isfinite(a) && std::isfinite(b) &&
            std::abs(a-b) <=
                1.0e-4 * std::max({1.0,std::abs(a),std::abs(b)});
    }

    [[nodiscard]] bool matches(
        PhysicalControlModelId selected,
        const std::vector<game::equipment::engine::MountedEngine>& mounted,
        const ShipParams& ship
    ) const noexcept
    {
        if (!valid || model != selected || count != mounted.size() ||
            !almost(shape.massKg,ship.massKg) ||
            !almost(shape.pitchInertiaKgM2,ship.pitchInertiaKgM2) ||
            !almost(shape.yawInertiaKgM2,ship.yawInertiaKgM2) ||
            !almost(shape.rollInertiaKgM2,ship.rollInertiaKgM2) ||
            !almost(shape.strafeAccel,ship.strafeAccel) ||
            !almost(shape.angularAccel,ship.angularAccel) ||
            !almost(shape.forwardMainEngineAccelerationMps2,
                    ship.forwardMainEngineAccelerationMps2) ||
            !almost(shape.reverseMainEngineAccelerationMps2,
                    ship.reverseMainEngineAccelerationMps2))
            return false;
        const auto checkVector=[&](glm::dvec3 a,glm::dvec3 b) {
            return almost(a.x,b.x) && almost(a.y,b.y) &&
                   almost(a.z,b.z);
        };
        if (!checkVector(shape.centerOfMassLocalMeters,
                         ship.centerOfMassLocalMeters) ||
            !checkVector(shape.physicalActuatorHalfExtentsMeters,
                         ship.physicalActuatorHalfExtentsMeters))
            return false;
        for (std::size_t i=0;i<count;++i)
        {
            const game::equipment::engine::Engine engine(mounted[i]);
            const double rated=engine.ratedThrustNewtons();
            const double capacity=rated>0.0
                ? engine.availableThrustNewtons()/rated : 0.0;
            const auto neutral=engine.execute({
                0.0,physicalControlSlots(selected)[i].
                    nominalForceDirectionBody,0u
            });
            if (!almost(rated,motors[i].ratedN) ||
                std::abs(capacity-motors[i].capacity01)>0.005 ||
                std::abs(neutral.availableGimbalRangeRad-
                         motors[i].coneRad)>0.005)
                return false;
        }
        return true;
    }

    // Invert the fixed 6x6 wrench Gram matrix exactly once when rebuilding
    // programs. The six rows map actual F/m and r x F/I, not invented motion.
    [[nodiscard]] bool buildInverse() noexcept
    {
        std::array<std::array<double, 2 * Axes>, Axes> augmented {};
        for (std::size_t i=0;i<Axes;++i)
        {
            for (std::size_t j=0;j<Axes;++j)
            {
                double sum=0.0;
                for (std::size_t k=0;k<3*count;++k)
                    sum+=wrench[i][k]*wrench[j][k];
                augmented[i][j]=sum+(i==j ? 1.0e-10 : 0.0);
            }
            augmented[i][i+Axes]=1.0;
        }
        for (std::size_t k=0;k<Axes;++k)
        {
            std::size_t pivot=k;
            for (std::size_t row=k+1;row<Axes;++row)
                if (std::abs(augmented[row][k])>
                    std::abs(augmented[pivot][k]))
                    pivot=row;
            if (std::abs(augmented[pivot][k])<1.0e-13)
                return false;
            if (pivot!=k) std::swap(augmented[pivot],augmented[k]);
            const double diagonal=augmented[k][k];
            for (double& value:augmented[k]) value/=diagonal;
            for (std::size_t row=0;row<Axes;++row)
            {
                if (row==k) continue;
                const double factor=augmented[row][k];
                for (std::size_t col=0;col<2*Axes;++col)
                    augmented[row][col]-=factor*augmented[k][col];
            }
        }
        for (std::size_t k=0;k<3*count;++k)
            for (std::size_t j=0;j<Axes;++j)
                for (std::size_t axis=0;axis<Axes;++axis)
                    inverse[k][j]+=
                        wrench[axis][k]*augmented[axis][j+Axes];
        return true;
    }

    // Alternating closed-form wrench correction and independent actuator
    // cone projection is used ONLY when generating/rebuilding the 12 basis
    // maneuvers, never on a normal flight control tick.
    [[nodiscard]] bool solve(
        std::size_t axis,double target,
        std::array<double, Variables>& result
    ) const noexcept
    {
        result.fill(0.0);
        double aftCap=0.0,foreCap=0.0;
        for (std::size_t i=0;i<count;++i)
            (motors[i].axisSign<0 ? aftCap : foreCap)+=
                motors[i].capacity01;
        const double balanced=std::min(aftCap,foreCap);
        for (std::size_t i=0;i<count;++i)
        {
            const double sum=motors[i].axisSign<0 ? aftCap : foreCap;
            if (sum>1.0e-9)
                result[3*i]=0.45*motors[i].capacity01*
                             balanced/sum;
        }
        const double normalizedTarget=target/scales[axis];
        // Bounded work on a damage/layout change, not on every physics frame.
        for (int iteration=0;iteration<144;++iteration)
        {
            std::array<double,Axes> residual {};
            residual[axis]=normalizedTarget;
            for(std::size_t row=0;row<Axes;++row)
                for(std::size_t col=0;col<3*count;++col)
                    residual[row]-=wrench[row][col]*result[col];
            double worst=0.0;
            for(double value:residual)
                worst=std::max(worst,std::abs(value));
            if (worst<2.5e-4) return true;
            for(std::size_t col=0;col<3*count;++col)
            {
                double delta=0.0;
                for(std::size_t row=0;row<Axes;++row)
                    delta+=inverse[col][row]*residual[row];
                result[col]+=delta;
            }
            for(std::size_t i=0;i<count;++i)
            {
                std::array<double,3> q {
                    result[3*i],result[3*i+1],result[3*i+2]
                };
                matrix_detail::projectThrustCone(
                    q,motors[i].capacity01,motors[i].coneRad
                );
                for(std::size_t j=0;j<3;++j)
                    result[3*i+j]=q[j];
            }
        }
        return false;
    }

    void build(
        PhysicalControlModelId selected,
        const std::vector<game::equipment::engine::MountedEngine>& mounted,
        const ShipParams& ship
    ) noexcept
    {
        *this={};
        model=selected;
        if (familyOf(model)!=PhysicalControlFamily::Assisted ||
            mounted.size()>Max ||
            physicalControlSlots(model).size()!=mounted.size() ||
            ship.massKg<=0.0)
            return;
        count=mounted.size();
        shape=ship;
        const auto& topology=physicalControlSlots(model);
        glm::dvec3 extents=ship.physicalActuatorHalfExtentsMeters;
        if (extents.x<=0.0 || extents.y<=0.0 || extents.z<=0.0)
            extents=glm::dvec3(std::max(1.0f,ship.turnRadius));
        const double inertias[3] {
            std::max(1.0,ship.pitchInertiaKgM2),
            std::max(1.0,ship.yawInertiaKgM2),
            std::max(1.0,ship.rollInertiaKgM2)
        };
        double totalRated=0.0;
        double bankForce[2] {};
        for (std::size_t i=0;i<count;++i)
        {
            const game::equipment::engine::Engine engine(mounted[i]);
            auto& m=motors[i];
            if (topology[i].role==EngineSlotRole::ManoeuvreCluster)
                continue;
            m.ratedN=engine.ratedThrustNewtons();
            m.capacity01=m.ratedN>0.0
                ? std::clamp(engine.availableThrustNewtons()/m.ratedN,
                             0.0,1.0) : 0.0;
            m.coneRad=engine.execute({
                0.0,topology[i].nominalForceDirectionBody,0u
            }).availableGimbalRangeRad;
            m.axisSign=topology[i].nominalForceDirectionBody.z>=0.0
                ? 1.0 : -1.0;
            m.arm=topology[i].normalizedPositionBody*extents-
                  ship.centerOfMassLocalMeters;
            totalRated+=m.ratedN;
            bankForce[m.axisSign<0.0?0:1]+=
                m.capacity01*m.ratedN;
        }
        scales={std::max(1.0,totalRated/ship.massKg),
                std::max(1.0,totalRated/ship.massKg),
                std::max(1.0,totalRated/ship.massKg),
                std::max(0.2,static_cast<double>(ship.angularAccel)),
                std::max(0.2,static_cast<double>(ship.angularAccel)),
                std::max(0.2,static_cast<double>(ship.angularAccel))};
        for(std::size_t i=0;i<count;++i)
        {
            if(motors[i].capacity01<=0.0) continue;
            const auto& m=motors[i];
            const glm::dvec3 forceAxes[3] {
                {0.0,0.0,m.axisSign}, {1.0,0.0,0.0},
                {0.0,1.0,0.0}
            };
            for(std::size_t axis=0;axis<3;++axis)
            {
                const auto f=forceAxes[axis]*m.ratedN;
                const auto torque=glm::cross(m.arm,f);
                const double values[6] {
                    f.x/ship.massKg,f.y/ship.massKg,f.z/ship.massKg,
                    torque.x/inertias[0],torque.y/inertias[1],
                    -torque.z/inertias[2]
                };
                for(std::size_t row=0;row<Axes;++row)
                    wrench[row][3*i+axis]=values[row]/scales[row];
            }
        }
        if (!buildInverse()) return;
        const double magnitudes[Axes] {
            std::max(0.0,static_cast<double>(ship.strafeAccel)),
            std::max(0.0,static_cast<double>(ship.strafeAccel)),
            std::max(
                static_cast<double>(ship.forwardMainEngineAccelerationMps2),
                static_cast<double>(ship.reverseMainEngineAccelerationMps2)
            ),
            std::max(0.0,static_cast<double>(ship.angularAccel)),
            std::max(0.0,static_cast<double>(ship.angularAccel)),
            std::max(0.0,static_cast<double>(ship.angularAccel))
        };
        for(std::size_t axis=0;axis<Axes;++axis)
            for(int direction=0;direction<2;++direction)
            {
                auto& program=programs[2*axis+direction];
                const double sign=direction==0 ? 1.0 : -1.0;
                double requested=magnitudes[axis];
                if (axis==2)
                    requested=bankForce[direction==0 ? 1 : 0]/
                              ship.massKg;
                if (requested<=1.0e-9) continue;
                // Healthy, geometrically centered fore/aft bank: a literal
                // equal-Newton signed AXIAL preset. This is the simplest
                // possible program, with EXACT zero nozzle tilt and no
                // artificial pitch/yaw/roll for Num8/Num2 or thrust.
                if (axis==2)
                {
                    const double signZ=direction==0 ? 1.0 : -1.0;
                    std::size_t active=0;
                    double weakest=std::numeric_limits<double>::infinity();
                    double sumX=0.0,sumY=0.0;
                    for(std::size_t i=0;i<count;++i)
                        if(motors[i].axisSign==signZ &&
                           motors[i].capacity01>1.0e-9)
                        {
                            ++active;
                            weakest=std::min(weakest,
                                motors[i].capacity01*motors[i].ratedN);
                            sumX+=motors[i].arm.x;
                            sumY+=motors[i].arm.y;
                        }
                    if(active>0 &&
                       std::abs(sumX)<1.0e-8 &&
                       std::abs(sumY)<1.0e-8)
                    {
                        const double equalN=std::min(
                            requested*ship.massKg/static_cast<double>(active),
                            weakest
                        );
                        for(std::size_t i=0;i<count;++i)
                            if(motors[i].axisSign==signZ &&
                               motors[i].capacity01>1.0e-9)
                                program.forceFractions[3*i]=
                                    equalN/motors[i].ratedN;
                        program.achievable=
                            equalN*static_cast<double>(active)/ship.massKg;
                        continue;
                    }
                }
                std::array<double,Variables> solution {};
                if(solve(axis,sign*requested,solution))
                {
                    program.forceFractions=solution;
                    program.achievable=requested;
                    continue;
                }
                // A destroyed/limited engine cannot deliver nominal commands.
                // Find a smaller SAFE program rather than accepting residual
                // off-axis acceleration or spin from an infeasible wrench.
                double lo=0.0,hi=1.0;
                for(int step=0;step<11;++step)
                {
                    const double fraction=(lo+hi)*0.5;
                    if(solve(axis,sign*requested*fraction,solution))
                    {
                        lo=fraction;
                        program.forceFractions=solution;
                        program.achievable=requested*fraction;
                    }
                    else
                        hi=fraction;
                }
            }
        valid=true;
    }

    [[nodiscard]] EngineMatrixResult execute(
        PhysicalControlModelId selected,
        const std::vector<game::equipment::engine::MountedEngine>& mounted,
        const ShipParams& ship,
        const EngineMatrixDemand& demand
    ) noexcept
    {
        if (!matches(selected,mounted,ship))
            build(selected,mounted,ship);
        EngineMatrixResult out;
        if(!valid) return out;
        out.valid=true;
        out.count=count;
        glm::dvec3 linearDemand = demand.accelerationBodyMps2;
        std::array<double,Variables> q {};

        // Emergency working axis: when one aft drive survives, the primary
        // translational thrust MUST pass through the CoM. An independent
        // pure-Z preset would otherwise push off-center and inject up to
        // 4.908 rad/s^2 unwanted yaw into the actual rigid body.
        // Build this exact physical ray before mixing the residual six-axis
        // correction. The surviving fore engines may still steer/trim.
        if (ship.assistedWorkingThrustAxisEmergency)
        {
            const glm::dvec3 axis = ship.assistedWorkingThrustAxisBody;
            const double requested = std::max(
                0.0, glm::dot(linearDemand, axis)
            );
            const auto& topology = physicalControlSlots(selected);
            for (std::size_t i=0;i<count && requested>1.0e-9;++i)
            {
                if (topology[i].role!=EngineSlotRole::AftMain ||
                    motors[i].capacity01<=1.0e-9 ||
                    motors[i].ratedN<=0.0)
                    continue;
                const double available =
                    motors[i].capacity01 * motors[i].ratedN;
                const double allocatedN = std::min(
                    available, requested*std::max(1.0,ship.massKg)
                );
                const double fraction = allocatedN/motors[i].ratedN;
                q[3*i] = std::max(0.0,-axis.z)*fraction;
                q[3*i+1] = axis.x*fraction;
                q[3*i+2] = axis.y*fraction;
                linearDemand -= axis *
                    (allocatedN/std::max(1.0,ship.massKg));
                // The mode is constructed only with exactly one aft motor.
                break;
            }
        }

        const double command[Axes] {
            linearDemand.x,
            linearDemand.y,
            linearDemand.z,
            demand.angularAccelerationBodyRadPerSec2.x,
            demand.angularAccelerationBodyRadPerSec2.y,
            demand.angularAccelerationBodyRadPerSec2.z
        };
        for(std::size_t axis=0;axis<Axes;++axis)
        {
            const double wanted=command[axis];
            // Ignore float-frame basis noise instead of injecting a tiny
            // undesired nozzle angle during a purely axial pilot command.
            if(!std::isfinite(wanted) || std::abs(wanted)<1.0e-4)
                continue;
            const auto& preset=programs[2*axis+(wanted<0.0 ? 1 : 0)];
            if(preset.achievable<=1.0e-9) continue;
            const double mix=std::min(
                1.0,std::abs(wanted)/preset.achievable
            );
            for(std::size_t i=0;i<3*count;++i)
                q[i]+=preset.forceFractions[i]*mix;
        }
        // Sums of positive thrust cones stay in the same cone. Only the
        // motor's nominal length limit can be exceeded by mixed commands.
        // Scale ALL motors equally: preserves requested force/torque balance.
        double globalScale=1.0;
        for(std::size_t i=0;i<count;++i)
        {
            const game::equipment::engine::Engine engine(mounted[i]);
            const double cap=motors[i].ratedN>0.0
                ? engine.instantaneousAvailableThrustNewtons(demand.tick)/
                  motors[i].ratedN : 0.0;
            const double magnitude=std::sqrt(
                q[3*i]*q[3*i]+q[3*i+1]*q[3*i+1]+
                q[3*i+2]*q[3*i+2]
            );
            if(magnitude>1.0e-12)
                globalScale=std::min(globalScale,
                    std::clamp(cap/magnitude,0.0,1.0));
        }
        if (ship.assistedWorkingThrustAxisEmergency)
        {
            // A safe requested attitude limit is not a license for an
            // asymmetrically mounted engine to inject an UNREQUESTED moment.
            // The precomputed wrench rows let us check the actual combined
            // motor command without adding a per-tick iterative solver.
            // Prefer losing thrust to creating runaway hull rotation.
            const double angularGoal[3] {
                demand.angularAccelerationBodyRadPerSec2.x,
                demand.angularAccelerationBodyRadPerSec2.y,
                demand.angularAccelerationBodyRadPerSec2.z
            };
            const double safeAngular =
                std::max(0.0, std::min(
                    static_cast<double>(ship.angularAccel),
                    ship.maxGs>0.0f && ship.turnRadius>0.0f
                        ? static_cast<double>(ship.maxGs)*9.80665/
                              static_cast<double>(ship.turnRadius)
                        : static_cast<double>(ship.angularAccel)
                ));
            for (std::size_t axis=0;axis<3;++axis)
            {
                double predicted=0.0;
                for (std::size_t k=0;k<3*count;++k)
                    predicted+=wrench[axis+3][k]*q[k];
                predicted*=scales[axis+3]*globalScale;
                const double desired=angularGoal[axis];
                // An accidental residual torque above numerical tolerance
                // is not "some rotation authority". Do not accept it.
                if (std::abs(desired)<0.01 &&
                    std::abs(predicted)>0.01)
                {
                    globalScale=0.0;
                    break;
                }
                if (desired*predicted < -1.0e-4)
                {
                    globalScale=0.0;
                    break;
                }
                const double allowable=std::min(
                    safeAngular, std::abs(desired)+0.01
                );
                if (std::abs(predicted)>allowable+1.0e-9)
                    globalScale*=allowable/std::abs(predicted);
            }
            // Pitch + yaw share a single crew/load angular envelope.
            double pitch=0.0,yaw=0.0;
            for(std::size_t k=0;k<3*count;++k)
            {
                pitch+=wrench[3][k]*q[k];
                yaw+=wrench[4][k]*q[k];
            }
            pitch*=scales[3]*globalScale;
            yaw*=scales[4]*globalScale;
            const double planar=std::hypot(pitch,yaw);
            if(planar>safeAngular && planar>1.0e-12)
                globalScale*=safeAngular/planar;
        }
        const double mass=std::max(1.0,ship.massKg);
        const glm::dvec3 inertia {
            std::max(1.0,ship.pitchInertiaKgM2),
            std::max(1.0,ship.yawInertiaKgM2),
            std::max(1.0,ship.rollInertiaKgM2)
        };
        for(std::size_t i=0;i<count;++i)
        {
            const game::equipment::engine::Engine engine(mounted[i]);
            auto& sample=out.engines[i];
            sample.availableThrust01=motors[i].ratedN>0.0
                ? std::clamp(engine.availableThrustNewtons()/
                         motors[i].ratedN,0.0,1.0)
                : 0.0;
            const glm::dvec3 normalized {
                q[3*i+1]*globalScale,
                q[3*i+2]*globalScale,
                q[3*i]*globalScale*motors[i].axisSign
            };
            const double magnitude=glm::length(normalized);
            if(magnitude<1.0e-12) continue;
            const double available=engine.instantaneousAvailableThrustNewtons(
                demand.tick
            );
            if(available<=1.0e-9) continue;
            const auto result=engine.execute({
                std::clamp(magnitude*motors[i].ratedN/available,0.0,1.0),
                normalized/magnitude,demand.tick
            });
            sample.deliveredThrust01=result.deliveredNominalThrust01;
            sample.forceBodyNewtons=result.forceBodyNewtons;
            sample.electricalMW=result.consumedElectricalMW;
            sample.heatMW=result.generatedWasteHeatMW;
            sample.nozzleYawRad=std::atan2(normalized.x,
                std::max(1.0e-12,std::abs(normalized.z)));
            sample.nozzlePitchRad=std::atan2(normalized.y,
                std::max(1.0e-12,std::abs(normalized.z)));
            out.forceBodyNewtons+=result.forceBodyNewtons;
            out.torqueBodyNm+=glm::cross(motors[i].arm,
                                        result.forceBodyNewtons);
        }
        out.accelerationBodyMps2=out.forceBodyNewtons/mass;
        out.angularAccelerationBodyRadPerSec2={
            out.torqueBodyNm.x/inertia.x,
            out.torqueBodyNm.y/inertia.y,
            -out.torqueBodyNm.z/inertia.z
        };
        return out;
    }
};
} // namespace game::ship::plant
