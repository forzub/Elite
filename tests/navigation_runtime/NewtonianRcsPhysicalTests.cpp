#include "src/game/equipment/rcs/presets/CobraArgonRcsTank.h"
#include "src/game/ship/plant/NewtonianRcsAllocator.h"
#include "src/game/equipment/engine/ManoeuvreEnginePreset.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition,const char* message)
{
    if(!condition) throw std::runtime_error(message);
}

void checkInstalledNozzleLoss()
{
    using namespace game::equipment;
    using namespace game::equipment::engine;
    std::array<EngineUnitDescriptor,24> passports;
    std::array<EquipmentDamageLayer,24> damage;
    std::array<EngineOperatingState,24> operating;
    std::array<MountedEngine,24> mounted;
    for(std::size_t i=0;i<24;++i)
    {
        passports[i]=cobraManoeuvreNozzlePreset();
        damage[i].failure.failureAtDamage01=0.95;
        operating[i].temperatureK=passports[i].nominalOperatingTemperatureK;
        mounted[i]={
            &passports[i],&damage[i],&operating[i],
            game::ship::presets::cobra::newtonianRcsNozzles()[i].nozzleId,
            true
        };
    }
    game::ship::plant::NewtonianRcsDemand command;
    command.forceBodyN={12000.0,5000.0,8000.0};
    command.momentBodyNm={40000.0,-25000.0,20000.0};
    command.mountHalfExtentsM={12.0,2.0,10.0};
    command.tick=1;
    const auto healthy=
        game::ship::plant::allocateNewtonianRcs(mounted,command);
    require(healthy.valid,"healthy bank not evaluated");
    double healthyLoad=0.0;
    std::size_t used=24;
    for(std::size_t i=0;i<24;++i)
    {
        healthyLoad+=healthy.throttle01[i];
        if(healthy.throttle01[i]>0.01 && used==24)
            used=i;
    }
    require(healthyLoad>0.01 && used<24,
            "allocator never fired a nozzle");
    damage[used].accumulated.impact01=1.0;
    const auto damaged=
        game::ship::plant::allocateNewtonianRcs(mounted,command);
    require(damaged.valid,"damaged bank not evaluated");
    require(damaged.availabilityN[used]==0.0,
            "destroyed nozzle still reports available thrust");
    require(damaged.throttle01[used]==0.0,
            "destroyed nozzle still commanded");
    require(damaged.electricalMW[used]==0.0,
            "destroyed nozzle still consumes electrical power");
    require(damaged.wasteHeatMW[used]==0.0,
            "destroyed nozzle still generates operating heat");
    require(std::isfinite(damaged.deliveredForceN.x) &&
            std::isfinite(damaged.deliveredMomentNm.z),
            "damaged wrench produced nonfinite values");
}
}
void checkCentreOfMassAndElectricalBudget()
{
    using namespace game::equipment;
    using namespace game::equipment::engine;
    std::array<EngineUnitDescriptor,24> passports;
    std::array<EquipmentDamageLayer,24> damage;
    std::array<EngineOperatingState,24> operating;
    std::array<MountedEngine,24> mounted {};
    for(std::size_t i=0;i<24;++i)
    {
        passports[i]=cobraManoeuvreNozzlePreset();
        operating[i].temperatureK=passports[i].nominalOperatingTemperatureK;
        mounted[i]={&passports[i],&damage[i],&operating[i],
            game::ship::presets::cobra::newtonianRcsNozzles()[i].nozzleId,
            i==0}; // All missing nozzles MUST contribute zero.
    }
    game::ship::plant::NewtonianRcsDemand command;
    command.forceBodyN={-8000.0,0.0,0.0};
    command.mountHalfExtentsM={12.0,2.0,10.0};
    command.centerOfMassLocalMeters={0.0,1.5,0.0};
    command.tick=11;
    const auto physical=game::ship::plant::allocateNewtonianRcs(mounted,command);
    require(physical.valid &&
            glm::length(physical.deliveredForceN)>1.0e-5,
            "remaining physical RCS nozzle did not fire");
    const auto& mount=game::ship::presets::cobra::newtonianRcsNozzles()[0];
    const glm::dvec3 arm=mount.positionBody*command.mountHalfExtentsM-
        command.centerOfMassLocalMeters;
    const glm::dvec3 expectedMoment=glm::cross(arm,physical.deliveredForceN);
    require(glm::length(physical.deliveredMomentNm-expectedMoment)<1.0e-7,
            "RCS torque was not computed about the CURRENT centre of mass");
    command.availableElectricalMW=0.0;
    const auto unpowered=game::ship::plant::allocateNewtonianRcs(mounted,command);
    require(unpowered.valid &&
            glm::length(unpowered.deliveredForceN)<1.0e-9 &&
            glm::length(unpowered.deliveredMomentNm)<1.0e-9 &&
            unpowered.throttle01[0]==0.0 &&
            unpowered.electricalMW[0]==0.0 &&
            unpowered.wasteHeatMW[0]==0.0,
            "unpowered installed RCS invented physical thrust or heat");
    require(unpowered.availabilityN[0]>0.0 &&
            unpowered.availabilityN[1]==0.0,
            "missing RCS nozzle was incorrectly treated as installed");
}
// Balanced nozzle heating: a 10 s full burst keeps full thrust, a 90 s full
// burn derates the nozzle (but keeps most of it), and it cools down again.
void checkBalancedNozzleHeating()
{
    using namespace game::equipment;
    using namespace game::equipment::engine;
    const auto passport=cobraManoeuvreNozzlePreset();
    EquipmentDamageLayer damage {};
    damage.failure.failureAtDamage01=0.95;
    EngineOperatingState operating {};
    operating.temperatureK=passport.nominalOperatingTemperatureK;
    const auto share=[&]()
    {
        const auto effective=evaluateEngineUnit(passport,damage,operating);
        return safeEmitterThrustFraction01(passport,effective,operating);
    };
    const auto burn=[&](double seconds,double throttle)
    {
        for(int tick=0;tick<static_cast<int>(seconds/0.2);++tick)
        {
            const auto effective=evaluateEngineUnit(passport,damage,operating);
            operating=advanceEngineOperatingState(
                passport,effective,operating,throttle,0.2,310.0).next;
        }
    };
    burn(10.0,1.0);
    require(share()>0.999,"a 10 s RCS burst already derated the nozzle");
    burn(80.0,1.0);
    const double hot=share();
    require(hot<0.95 && hot>0.55,
            "a 90 s full RCS burn did not derate the nozzle moderately");
    burn(300.0,0.0);
    require(share()>hot,"a resting nozzle did not cool down");
}

void checkCombatNozzlePowerAndThermalPlateau()
{
    using namespace game::equipment;
    using namespace game::equipment::engine;
    const auto passport=cobraManoeuvreNozzlePreset();
    EquipmentDamageLayer damage {};
    damage.failure.failureAtDamage01=0.95;
    EngineOperatingState operating {};
    operating.temperatureK=passport.nominalOperatingTemperatureK;
    const MountedEngine mounted {
        &passport,&damage,&operating,"rcs_test",true
    };
    const auto output=Engine(mounted).execute(
        EngineCommand {1.0,{1.0,0.0,0.0},1});
    require(std::abs(output.deliveredThrustNewtons-500000.0)<1.0 &&
            std::abs(output.consumedElectricalMW-20.0)<1.e-5,
        "healthy new 500kN RCS did not execute its real 20MW passport");
    for(int tick=0;tick<300;++tick)
    {
        const auto effective=evaluateEngineUnit(passport,damage,operating);
        const auto step=advanceEngineOperatingState(
            passport,effective,operating,1.0,0.2,310.0);
        operating=step.next;
    }
    require(std::isfinite(operating.temperatureK) &&
            operating.temperatureK<passport.safeOperatingTemperatureK,
        "RCS physical radiator cannot cool 60 seconds at rated combat duty");
}

// The manifold refills only while the nozzles draw nothing: even a small
// continuous draw consumes the stored gas, recovery starts when it stops.
void checkGasRecoversOnlyWithoutConsumption()
{
    double reservoir=0.5;
    bool depleted=false;
    for(int i=0;i<100;++i)
        (void)game::ship::plant::advanceNewtonianRcsGas(
            reservoir,depleted,0.02,0.05,0.20,0.08,0.20);
    require(reservoir<0.5,
            "a small continuous RCS draw was refilled by the compressor");
    const double afterDraw=reservoir;
    for(int i=0;i<100;++i)
        (void)game::ship::plant::advanceNewtonianRcsGas(
            reservoir,depleted,0.02,0.0,0.20,0.08,0.20);
    require(reservoir>afterDraw+0.15,
            "the manifold did not recover once the consumption stopped");
}

// The installed tank: damage shrinks the usable capacity, a failed or
// destroyed vessel holds no gas, the regulator weakens the nozzles only
// below its knee and an emptied tank gives no thrust.
void checkRcsGasTankPassportLaw()
{
    using namespace game::equipment::rcs;
    const auto& tank = cobraArgonRcsTank();
    game::equipment::EquipmentDamageState healthy {};
    require(rcsGasTankCapacity01(tank, healthy, true) == 1.0,
            "a healthy tank does not hold its rated gas");
    game::equipment::EquipmentDamageState hit {};
    hit.impact01 = 0.5;
    const double damaged = rcsGasTankCapacity01(tank, hit, true);
    require(damaged < 1.0 && damaged > 0.0,
            "a damaged tank kept its full capacity or lost all of it");
    hit.impact01 = 1.0;
    require(rcsGasTankCapacity01(tank, hit, true) == 0.0 &&
            rcsGasTankCapacity01(tank, healthy, false) == 0.0,
            "a failed or destroyed tank still feeds the RCS");
    require(rcsGasThrustShare01(tank, 0.9, false) == 1.0 &&
            rcsGasThrustShare01(tank, 0.5 * tank.regulatorFullThrustFraction,
                false) < 0.51 &&
            rcsGasThrustShare01(tank, 0.9, true) == 0.0,
            "the regulator law does not follow the tank pressure");
}

void checkReplenishingGameGasBeforeFire()
{
    double reservoir=1.0;
    bool depleted=false;
    double totalBurn=0.0;
    for(int i=0;i<700;++i)
    {
        const double authority=game::ship::plant::advanceNewtonianRcsGas(
            reservoir,depleted,0.02,1.0,0.20,0.08,0.20);
        totalBurn+=authority;
    }
    require(depleted && reservoir>=0.0 && reservoir<0.20 &&
            totalBurn>0.0,
            "sustained RCS burn did not exhaust the shared game gas");
    bool rearmed=false;
    for(int i=0;i<200;++i)
    {
        const double authority=game::ship::plant::advanceNewtonianRcsGas(
            reservoir,depleted,0.02,0.0,0.20,0.08,0.20);
        if(!depleted)
        {
            rearmed=true;
            require(authority==0.0,
                    "RCS fired during the latched recharge tick");
            break;
        }
        require(authority==0.0,
                "depleted gas allowed a physical nozzle demand");
    }
    require(rearmed,"replenishing RCS reservoir never restarted");
    const double resumed=game::ship::plant::advanceNewtonianRcsGas(
        reservoir,depleted,0.02,1.0,0.20,0.08,0.20);
    require(resumed>0.0,"RCS did not resume after reservoir recovery");
}
void checkSignedPhysicalAuthorityProjection()
{
    using namespace game::equipment;
    using namespace game::equipment::engine;
    std::array<EngineUnitDescriptor,24> passports {};
    std::array<EquipmentDamageLayer,24> damage {};
    std::array<EngineOperatingState,24> operating {};
    std::array<MountedEngine,24> installed {};
    const auto& catalog=game::ship::presets::cobra::newtonianRcsNozzles();
    for(std::size_t i=0;i<installed.size();++i)
    {
        passports[i]=cobraManoeuvreNozzlePreset();
        damage[i].failure.failureAtDamage01=0.95;
        operating[i].temperatureK=
            passports[i].nominalOperatingTemperatureK;
        installed[i]={
            &passports[i],&damage[i],&operating[i],
            catalog[i].nozzleId,true
        };
    }
    const glm::dvec3 halfExtents={12.0,2.5,10.0};
    const auto healthy=
        game::ship::plant::sampleNewtonianRcsSignedAuthority(
            installed,halfExtents,{0.0,0.0,0.0});
    for(int axis=0;axis<3;++axis)
    {
        require(healthy.positiveForceN[axis]>0.0 &&
                healthy.negativeForceN[axis]>0.0,
            "healthy Newtonian has no physical bidirectional RCS translation");
        require(healthy.positiveMomentNm[axis]>0.0 &&
                healthy.negativeMomentNm[axis]>0.0,
            "healthy Newtonian has no physical bidirectional RCS torque");
        require(healthy.positiveForceN[axis]<=12000000.0+1.e-3 &&
                healthy.negativeForceN[axis]<=12000000.0+1.e-3,
            "RCS authority advertises more than all installed nozzle force");
    }
    // Test real simultaneous 2-axis manual turns, not just isolated
    // pitch/yaw/roll. A manoeuvrable spacecraft must not serialize a
    // modest pitch+roll, yaw+roll, or pitch+yaw into separate burns.
    // Stay well within single-axis attainable moments so the assertion
    // does not demand impossible sum of full independent maxima.
    for(const auto pair: {
        std::array<int,2>{0,2},
        std::array<int,2>{1,2},
        std::array<int,2>{0,1}
    })
    {
        game::ship::plant::NewtonianRcsDemand combined;
        combined.mountHalfExtentsM=halfExtents;
        combined.availableElectricalMW=10.0;
        combined.tick=4;
        for(const int axis:pair)
            combined.momentBodyNm[axis]=
                0.15*healthy.positiveMomentNm[axis];
        const auto actual=game::ship::plant::allocateNewtonianRcs(
            installed,combined);
        require(actual.valid,
            "simultaneous Newtonian angular wrench was rejected");
        for(const int axis:pair)
            require(actual.deliveredMomentNm[axis]>
                    0.10*combined.momentBodyNm[axis],
                "two-axis RCS request lost one requested rotation");
    }
    // A wholly destroyed RCS bank cannot advertise ANY free controller
    // authority: 2m/s2/3rad/s2 descriptor intent is not physical evidence.
    for(auto& d:damage)d.accumulated.impact01=1.0;
    const auto destroyed=
        game::ship::plant::sampleNewtonianRcsSignedAuthority(
            installed,halfExtents,{0.0,0.0,0.0});
    for(int axis=0;axis<3;++axis)
    {
        require(destroyed.positiveForceN[axis]==0.0 &&
                destroyed.negativeForceN[axis]==0.0 &&
                destroyed.positiveMomentNm[axis]==0.0 &&
                destroyed.negativeMomentNm[axis]==0.0,
            "destroyed installed RCS resurrected nominal free authority");
    }
}
int main()
{
    try
    {
        checkInstalledNozzleLoss();
        checkCombatNozzlePowerAndThermalPlateau();
        checkCentreOfMassAndElectricalBudget();
        checkReplenishingGameGasBeforeFire();
        checkGasRecoversOnlyWithoutConsumption();
        checkRcsGasTankPassportLaw();
        checkBalancedNozzleHeating();
        checkSignedPhysicalAuthorityProjection();
        std::cout<<"NEWTONIAN RCS PHYSICAL: PASS\n";
        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr<<"NEWTONIAN RCS PHYSICAL: FAIL: "<<e.what()<<'\n';
        return 1;
    }
}
