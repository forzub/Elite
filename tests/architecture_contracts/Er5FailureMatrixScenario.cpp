#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include "src/world/modules/ObjectModuleState.h"
#include "src/game/equipment/reactor/presets/CobraCobrettyER5PropulsionReactor.h"
#include "src/game/physics/UniverseThermalRadiation.h"

using game::equipment::reactor::advanceEngineReactor;
using game::equipment::reactor::presets::CobraCobrettyER5PropulsionReactor;
using world::modules::ObjectModuleState;

struct Case {
    const char* name;
    double pad01;
    double internalDamage01;
    double demandMW;
    bool ignition;
};
static void require(bool yes, const char* why) {
    if(!yes) throw std::runtime_error(why);
}
int main() {
    try {
        std::error_code ec;
        std::filesystem::create_directories("D:/tmp",ec);
        const std::string path="D:/tmp/elite_er5_failure_matrix.csv";
        std::ofstream file(path,std::ios::trunc);
        require(file.good(),"cannot open D:/tmp/elite_er5_failure_matrix.csv");
        file<<"case,seconds,ignition,attached,module_health,pad_integrity01,internal_damage01,requested_MW,actual_MW,reactor_T_K,generated_MW,pad_radiated_MW,emergency_MW,stored_MW,failed,heat_balance_error_MW\n";
        file<<std::fixed<<std::setprecision(6);
        const auto passport=CobraCobrettyER5PropulsionReactor();
        const Case cases[]={
            {"healthy_nominal",1.0,0.0,4200.0,true},
            {"power_off_intact",1.0,0.0,4200.0,false},
            {"pad_half_area",0.5,0.0,4200.0,true},
            {"pad_critical_10pct",0.1,0.0,4200.0,true},
            {"pad_destroyed",0.0,0.0,4200.0,true},
            {"reactor_internal_failed",1.0,1.0,4200.0,true}
        };
        for(const auto& scenario:cases) {
            // Same storage used by the installed runtime module, not a HUD fake.
            // The test does NOT simulate collision geometry or network traffic.
            ObjectModuleState module;
            module.moduleId="diagnostic_er5_0";
            module.health=100.0f;
            module.engineReactorOperating.padIntegrity01=scenario.pad01;
            module.engineReactorOperating.temperatureK=passport.idleTemperatureK;
            module.equipmentDamage.accumulated.internalFailure01=scenario.internalDamage01;
            const double dt=1.0;
            double finalT=0,finalPower=0,finalPad=0;
            for(int t=0;t<=1800;++t) {
                auto& state=module.engineReactorOperating;
                state.damage=module.equipmentDamage.accumulated;
                const double ratedPad=
                    game::physics::thermal::PropulsionPadCoolingMWPerM2*
                    passport.padSideM*passport.padSideM*
                    std::clamp(state.padIntegrity01,0.0,1.0);
                const auto tick=advanceEngineReactor(passport,state,
                    scenario.ignition?scenario.demandMW:0.0,
                    ratedPad,0.0,dt);
                const double residual=state.lastGeneratedHeatMW-
                    state.lastPadHeatMW-state.lastCoolantHeatMW-
                    state.lastStoredHeatMW;
                require(std::isfinite(state.temperatureK),"invalid reactor T");
                require(std::abs(residual)<1e-5,"thermal channels unbalanced");
                require(state.padIntegrity01==scenario.pad01,
                    "pad integrity mutated without a hit");
                require(tick.overflowToSharedCoolantMW==0.0,
                    "phantom emergency bridge");
                if(t%10==0||t==1800) {
                    file<<scenario.name<<','<<t<<','<<(scenario.ignition?1:0)
                        <<",1,"<<module.health<<','<<state.padIntegrity01
                        <<','<<scenario.internalDamage01<<','
                        <<scenario.demandMW<<','<<tick.electricalOutputMW
                        <<','<<state.temperatureK<<','
                        <<state.lastGeneratedHeatMW<<','
                        <<state.lastPadHeatMW<<','
                        <<state.lastCoolantHeatMW<<','
                        <<state.lastStoredHeatMW<<','
                        <<(tick.failed?1:0)<<','<<residual<<'\n';
                }
                finalT=state.temperatureK;
                finalPower=tick.electricalOutputMW;
                finalPad=state.lastPadHeatMW;
            }
            if(std::string(scenario.name)=="power_off_intact" ||
               std::string(scenario.name)=="reactor_internal_failed")
                require(finalPower==0.0,"off or failed ER-5 produced electricity");
            if(std::string(scenario.name)=="pad_destroyed")
                require(finalPad==0.0,"destroyed pad radiated heat");
            std::cout<<scenario.name<<" T="<<finalT<<" K P="<<finalPower
                <<" MW pad="<<finalPad<<" MW\n";
        }
        file.flush();
        require(file.good(),"CSV write failed");
        std::cout<<"ER5 FAILURE MATRIX: PASS | "<<path<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"ER5 FAILURE MATRIX: FAIL: "<<e.what()<<'\n';
        return 1;
    }
}
