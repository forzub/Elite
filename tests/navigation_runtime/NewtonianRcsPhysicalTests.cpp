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
int main()
{
    try
    {
        checkInstalledNozzleLoss();
        std::cout<<"NEWTONIAN RCS PHYSICAL: PASS\n";
        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr<<"NEWTONIAN RCS PHYSICAL: FAIL: "<<e.what()<<'\n';
        return 1;
    }
}
