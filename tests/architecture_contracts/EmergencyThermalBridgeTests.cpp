#include <cmath>
#include <iostream>
#include "src/game/equipment/reactor/EmergencyThermalBridge.h"
#include "src/game/equipment/reactor/EngineReactorUnit.h"
#include "src/game/equipment/reactor/presets/CobraCobrettyER5PropulsionReactor.h"
using namespace game::equipment::reactor;
namespace {
int failures=0;
void check(bool value,const char* message) {
    if(!value){std::cerr<<"THERMAL BRIDGE FAIL: "<<message<<"\n"; ++failures;}
}
}
int main() {
    EmergencyThermalBridgeDescriptor d;
    EmergencyThermalBridgeState state;
    EmergencyThermalBridgeInput in;
    in.sourceK=2200; in.sinkK=900; in.authority01=1;
    in.pumpPowerAvailableMW=3; in.sinkHeadroomMW=25;
    in.sourceExtractableMW=100; in.sinkOpenBelowK=1100;
    in.sinkCloseAboveK=1150;
    auto tick=advanceEmergencyThermalBridge(
        d,state,EmergencyThermalBridgeMode::Auto,in);
    check(tick.open,"auto should open hot source");
    check(std::abs(tick.transferMW-25)<1e-9,"must obey sink headroom");
    check(tick.pumpElectricalMW<=3,"pump over its power budget");
    in.sinkHeadroomMW=0;
    tick=advanceEmergencyThermalBridge(
        d,state,EmergencyThermalBridgeMode::ManualOpen,in);
    check(tick.transferMW==0,"saturated radiator must reject bridge load");
    in.sinkHeadroomMW=100; in.sinkK=1200;
    tick=advanceEmergencyThermalBridge(
        d,state,EmergencyThermalBridgeMode::ManualOpen,in);
    check(tick.transferMW==0 && !tick.open,"manual must respect hot-sink interlock");
    in.sinkK=900; in.authority01=0;
    tick=advanceEmergencyThermalBridge(
        d,state,EmergencyThermalBridgeMode::ManualOpen,in);
    check(tick.transferMW==0 && !tick.open,"destroyed bridge still operated");
    in.authority01=1; in.sourceK=910; in.sinkK=900;
    tick=advanceEmergencyThermalBridge(
        d,state,EmergencyThermalBridgeMode::ManualOpen,in);
    check(std::abs(tick.transferMW-2.5)<1e-9,"conductance limit");
    in.sourceK=890;
    tick=advanceEmergencyThermalBridge(
        d,state,EmergencyThermalBridgeMode::ManualOpen,in);
    check(tick.transferMW==0,"heat moved against temperature gradient");

    auto er=presets::CobraCobrettyER5PropulsionReactor();
    EngineReactorState stateOff;
    stateOff.temperatureK=2300;
    EngineReactorState stateOn=stateOff;
    const double dt=0.02;
    const double electric=4000.0;
    const double pad=0.25*6.0*er.padSideM*er.padSideM;
    const auto off=advanceEngineReactor(er,stateOff,electric,pad,0,dt);
    const auto on=advanceEngineReactor(er,stateOn,electric,pad,60,dt);
    check(off.overflowToSharedCoolantMW==0,"default bridge moved heat");
    check(on.overflowToSharedCoolantMW>0 &&
          on.overflowToSharedCoolantMW<=60,"unbounded ER5 transfer");
    check(std::abs((stateOff.temperatureK-stateOn.temperatureK)*
          er.thermalCapacityMJPerK -
          on.overflowToSharedCoolantMW*dt)<1e-6,
          "ER5 transfer energy not conserved");
    check(std::abs(on.lastGeneratedHeatMW-on.lastPadHeatMW-
          on.lastCoolantHeatMW-on.lastStoredHeatMW)<1e-6,
          "ER5 per-tick heat channels do not balance");
    EngineReactorState atIdle;
    atIdle.temperatureK=er.idleTemperatureK;
    const auto idle=advanceEngineReactor(er,atIdle,0,
        6.0*er.padSideM*er.padSideM,1e6,dt);
    check(idle.overflowToSharedCoolantMW<=idle.thermalPowerMW+1e-9,
          "bridge extracted unavailable thermal energy at idle");
    if(failures)return 1;
    std::cout<<"EMERGENCY THERMAL BRIDGE: PASS\n";
    return 0;
}
