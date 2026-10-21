#include "src/game/ship/capability/WorkingThrustAxisAnalysis.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace game::ship::capability;

namespace
{
void check(bool ok,const char* reason)
{
    if(!ok) throw std::runtime_error(reason);
}

ForwardMotorGeometry motor(
    std::size_t id,double x,double y,double z,
    double capacity,double coneDeg=75.0
)
{
    return {id,{x,y,z},capacity,
        coneDeg*3.14159265358979323846/180.0};
}

void testHealthyIYAndX()
{
    const auto a=analyzeWorkingThrustGeometry({
        motor(0,-13.0,0.0,11.1,9.56e6),
        motor(1, 13.0,0.0,11.1,9.56e6)
    });
    check(a.centeredForwardAvailable && !a.offAxis,
        "healthy I should keep nose as available zero-torque axis");

    const auto y=analyzeWorkingThrustGeometry({
        motor(0,0.0,2.5,11.1,8.0e6),
        motor(1,-11.26,-1.25,11.1,8.0e6),
        motor(2,11.26,-1.25,11.1,8.0e6)
    });
    check(y.centeredForwardAvailable && !y.offAxis,
        "healthy Y should keep nose axis");

    const auto x=analyzeWorkingThrustGeometry({
        motor(0,-9.2,1.77,11.1,8.0e6),
        motor(1,9.2,1.77,11.1,8.0e6),
        motor(2,-9.2,-1.77,11.1,8.0e6),
        motor(3,9.2,-1.77,11.1,8.0e6)
    });
    check(x.centeredForwardAvailable && !x.offAxis,
        "healthy X should keep nose axis");
}

void testEmergencyIWithDamagedOrDeadLeft()
{
    const auto dead=analyzeWorkingThrustGeometry({
        motor(0,-13,0,11.1,0.0),
        motor(1,13,0,11.1,9561483.75)
    });
    check(!dead.centeredForwardAvailable && dead.offAxis &&
          dead.preferredAftSlot==1 &&
          dead.forceDirectionBody.x<0.0 &&
          dead.forceDirectionBody.z<0.0,
          "right-survivor exhaustion/force nozzle signs reversed");
    const double degrees=180.0*dead.axisOffsetRad/
        3.14159265358979323846;
    check(std::abs(degrees-49.5)<0.2,
        "right aft 13m/11.1m COM ray angle incorrect");
    const auto partial=analyzeWorkingThrustGeometry({
        motor(0,-13,0,11.1,9561483.75*0.5),
        motor(1,13,0,11.1,9561483.75)
    });
    check(partial.centeredForwardAvailable && !partial.offAxis,
        "two asymmetric but throttle-balanceable aft motors must retain nose");
}

void testYTwoRemainingWithNoStraightBalance()
{
    const auto y=analyzeWorkingThrustGeometry({
        motor(0,0.0,2.5,11.1,9561483.75),
        motor(1,-11.26,-1.25,11.1,9561483.75),
        motor(2,11.26,-1.25,11.1,0.0)
    });
    check(!y.centeredForwardAvailable && y.offAxis,
        "Y missing aft leg must have non-axial COM trim");
    check(y.preferredAftSlot==0 || y.preferredAftSlot==1,
        "Y engine-out chose a destroyed aft motor");
}

void testOppositePairAndUnreachable()
{
    const auto x=analyzeWorkingThrustGeometry({
        motor(0,-9.2,1.77,11.1,9561483.75),
        motor(1,9.2,1.77,11.1,0.0),
        motor(2,-9.2,-1.77,11.1,0.0),
        motor(3,9.2,-1.77,11.1,9561483.75*0.3)
    });
    check(x.centeredForwardAvailable && !x.offAxis,
        "opposite X pair should balance moments even with unequal capacity");

    const auto fixed=analyzeWorkingThrustGeometry({
        motor(0,-13,0,11.1,0.0),
        motor(1,13,0,11.1,9561483.75,10.0)
    });
    check(fixed.hasUsableThrust &&
          !fixed.offAxis && !fixed.centeredForwardAvailable,
          "insufficient physical gimbal must NOT invent feasible cruise axis");
    const auto missing=analyzeWorkingThrustGeometry({
        motor(0,-13,0,11.1,0.0),
        motor(1,13,0,11.1,0.0)
    });
    check(!missing.hasUsableThrust &&
          !missing.centeredForwardAvailable &&
          !missing.offAxis,
          "destroyed bank reported physical forward propulsion");
}

} // namespace

int main()
{
    try
    {
        testHealthyIYAndX();
        testEmergencyIWithDamagedOrDeadLeft();
        testYTwoRemainingWithNoStraightBalance();
        testOppositePairAndUnreachable();
        std::cout<<"WORKING THRUST AXIS ANALYSIS: PASS\n";
        return EXIT_SUCCESS;
    }
    catch(const std::exception& error)
    {
        std::cerr<<"WORKING THRUST AXIS ANALYSIS: FAIL: "
                 <<error.what()<<"\n";
        return EXIT_FAILURE;
    }
}
