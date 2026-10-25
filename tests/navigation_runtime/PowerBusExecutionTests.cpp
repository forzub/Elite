// Real production PowerBus/ReactorSystem integration, not a cloned power law.
#include "src/game/ship/core/PowerBus.h"
#include "src/game/ship/core/ReactorSystem.h"
#include "src/game/equipment/capabilities/IPowerConsumer.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void check(bool ok, const char* message)
{
    if(!ok) throw std::runtime_error(message);
}
bool close(double a,double b,double tol=1.0e-7)
{
    return std::isfinite(a) && std::isfinite(b) &&
        std::abs(a-b)<=tol;
}
struct Consumer final: game::equipment::IPowerConsumer
{
    double request=0.0;
    double granted=0.0;
    game::equipment::PowerPriority priority=
        game::equipment::PowerPriority::Support;
    Consumer(double mw,game::equipment::PowerPriority p)
        :request(mw),priority(p) {}
    double getRequestedPower() const override {return request;}
    void setAvailablePower(double mw) override {granted=mw;}
    double getAvailablePower() const override {return granted;}
    game::equipment::PowerPriority getPriority() const override
    {
        return priority;
    }
};
game::ship::ReactorDescriptor reactorDescriptor(double peakMW)
{
    game::ship::ReactorDescriptor d {};
    d.nominalPowerMW=peakMW;
    d.peakPowerMW=peakMW;
    d.optimalTempK=1400.0;
    d.minTempK=1300.0;
    d.maxTempK=2000.0;
    d.temperature=1500.0;
    d.thermalMass=30.0;
    d.efficiencyMinTemp=0.35;
    d.efficiencyPeak=0.45;
    d.efficiencyMaxTemp=0.40;
    return d;
}
void normalDemandAllocatesCurrentPower()
{
    using namespace game::ship::core;
    using game::equipment::PowerPriority;
    ReactorSystem reactor(reactorDescriptor(100.0));
    reactor.setThrottle(0.0);
    PowerBus bus(reactor);
    Consumer life(10.0,PowerPriority::Critical);
    Consumer cooling(20.0,PowerPriority::Critical);
    Consumer sensors(25.0,PowerPriority::Combat);
    bus.registerConsumer(&life);
    bus.registerConsumer(&cooling);
    bus.registerConsumer(&sensors);
    bus.setAuxiliaryDemandMW(5.0);

    bus.update(true);
    check(close(life.granted,10.0)&&close(cooling.granted,20.0)&&
            close(sensors.granted,25.0),
          "normal bus used PRE-throttle stale zero reactor output");
    check(reactor.getCurrentOutput()>=60.0 &&
          reactor.getCurrentOutput()<=100.0 && !bus.overloaded(),
          "normal bus failed to request measured service+RCS energy");

    bus.update(false);
    check(close(reactor.getThrottle(),0.0)&&
          close(reactor.getCurrentOutput(),0.0)&&
          close(life.granted,0.0)&&close(cooling.granted,0.0)&&
          close(sensors.granted,0.0) && !bus.overloaded(),
          "all-reactors OFF still powered service consumers");
    bus.update(false); // Still in startup/off gate; no silent restart.
    check(close(reactor.getThrottle(),0.0),
          "service reactor auto-started during explicit startup gate");
    bus.update(true);
    check(close(life.granted,10.0)&&close(cooling.granted,20.0)&&
          close(sensors.granted,25.0),
          "service reactor did not restore consumer power after startup");
}
void shortageAndZeroCapacity()
{
    using namespace game::ship::core;
    using game::equipment::PowerPriority;
    ReactorSystem reactor(reactorDescriptor(100.0));
    PowerBus bus(reactor);
    Consumer a(60.0,PowerPriority::Support);
    Consumer b(60.0,PowerPriority::Combat);
    bus.registerConsumer(&a);
    bus.registerConsumer(&b);
    bus.update();
    check(close(a.granted,50.0)&&close(b.granted,50.0)&&bus.overloaded(),
          "finite reactor did not proportionally reduce excess demand");

    ReactorSystem dead(reactorDescriptor(0.0));
    PowerBus zero(dead);
    Consumer critical(5.0,PowerPriority::Critical);
    zero.registerConsumer(&critical);
    zero.update();
    check(close(dead.getThrottle(),0.0)&&
          close(dead.getCurrentOutput(),0.0)&&
          close(critical.granted,0.0) && zero.overloaded(),
          "zero-capacity reactor produced nonfinite or phantom power");
}
void emergencyPrioritizesCritical()
{
    using namespace game::ship::core;
    using game::equipment::PowerPriority;
    auto desc=reactorDescriptor(100.0);
    desc.temperature=1950.0; // Inside the existing emergency envelope.
    ReactorSystem reactor(desc);
    reactor.setEmergencyPowerLimit(12.0);
    reactor.updateEmergencyMode();
    check(reactor.isEmergencyMode(),
          "emergency fixture did not enter emergency mode");
    PowerBus bus(reactor);
    Consumer life(6.0,PowerPriority::Critical);
    Consumer cooling(10.0,PowerPriority::Critical);
    Consumer optional(20.0,PowerPriority::Support);
    bus.registerConsumer(&life);
    bus.registerConsumer(&cooling);
    bus.registerConsumer(&optional);
    bus.update();
    check(close(life.granted,6.0)&&close(cooling.granted,6.0)&&
          close(optional.granted,0.0)&&bus.overloaded(),
          "emergency power did not reserve the 12 MW for critical consumers");
}
}
int main()
{
    try
    {
        normalDemandAllocatesCurrentPower();
        shortageAndZeroCapacity();
        emergencyPrioritizesCritical();
        std::cout<<"SHIP POWER BUS EXECUTION: PASS\n";
        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr<<"SHIP POWER BUS EXECUTION: FAIL: "<<e.what()<<'\n';
        return 1;
    }
}
