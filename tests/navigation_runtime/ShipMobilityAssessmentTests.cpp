#include "src/game/ship/capability/ShipMobilityAssessment.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
using namespace game::ship::capability;

void require(bool condition, const char* message)
{
    if(!condition)
        throw std::runtime_error(message);
}

ShipMobilityEvidence nominal()
{
    ShipMobilityEvidence e;
    e.control.verified = true;
    e.control.canProvideControlledThrust = true;
    e.control.canArrestAngularVelocity = true;
    e.control.canCompleteHalfTurn = true;
    e.control.canHoldRequiredBurnAttitude = true;
    e.control.canBrakeAfterReorientation = true;
    e.control.fullAttitudeAuthority = true;
    e.control.controlledCourseAuthority = true;
    e.control.factoryPerformanceEnvelope = true;
    e.control.safeManeuverDurationSeconds = 60.0;
    e.control.controlledAccelerationMps2 = 25.0;

    e.power.verified = true;
    e.power.continuouslyAvailableMW = 12000.0;
    e.power.essentialSystemsMW = 400.0;
    e.power.maneuverActuatorsMW = 8000.0;
    e.power.maximumStorageDischargeMW = 0.0;
    e.power.usableStoredEnergyMJ = 0.0;

    e.thermal.verified = true;
    e.thermal.maneuverHeatInputMW = 150.0;
    e.thermal.sustainableHeatRejectionMW = 160.0;
    e.thermal.remainingSafeHeatStorageMJ = 0.0;
    return e;
}

void testUnknownResourceDataCannotAuthorizeFlight()
{
    auto e = nominal();
    e.power.verified = false;
    auto a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Unverified,
            "missing electric evidence claimed return flight");
    require(!a.candidateForSimpleReturnRoute,
            "unverified ship was authorized for routing");
    e = nominal();
    e.thermal.verified = false;
    a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Unverified,
            "missing cooling evidence claimed return flight");
    e = nominal();
    e.control.verified = false;
    a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Unverified,
            "missing actual signed-wrench evidence claimed control");
}

void testFactoryAndLimited()
{
    auto e = nominal();
    auto a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Factory &&
            a.candidateForSimpleReturnRoute,
            "factory capability not recognized");

    e.control.factoryPerformanceEnvelope = false;
    a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Limited &&
            a.survivalManeuverCertified,
            "safely controlled but diminished ship not limited");
}

void testSingleEngineProofMayAuthorizeOnlyEmergency()
{
    auto e = nominal();
    // No assumption about how many motors, their layout or law.
    // This evidence is supplied only after an independent physical test
    // has shown thrust, attitude rotation/arrest and reverse-velocity brake.
    e.control.fullAttitudeAuthority = false;
    e.control.controlledCourseAuthority = false;
    e.control.factoryPerformanceEnvelope = false;
    e.control.controlledAccelerationMps2 = 3.0;
    e.control.safeManeuverDurationSeconds = 300.0;

    auto a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Emergency &&
            a.candidateForSimpleReturnRoute,
            "verified survival maneuver must remain eligible for route proof");

    e.control.canArrestAngularVelocity = false;
    a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Inoperable &&
            !a.candidateForSimpleReturnRoute,
            "half-turn without stopping spin was treated as flyable");
}

void testTransientElectricAndCoolingBudget()
{
    auto e = nominal();
    e.control.safeManeuverDurationSeconds = 80.0;
    e.power.continuouslyAvailableMW = 5000.0;
    e.power.maximumStorageDischargeMW = 3400.0;
    e.power.usableStoredEnergyMJ = 80.0 * 3400.0;
    // 8400 - 5000 = 3400 MW; emergency storage lasts exactly 80s.
    auto a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Emergency &&
            !a.continuousPowerSustainable &&
            a.survivalManeuverCertified,
            "temporary electrical storage was mistaken for factory flight");
    e.power.usableStoredEnergyMJ = 79.0 * 3400.0;
    a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Inoperable,
            "electric storage exhausted before maneuver completed");

    e = nominal();
    e.control.safeManeuverDurationSeconds = 80.0;
    e.thermal.maneuverHeatInputMW = 250.0;
    e.thermal.sustainableHeatRejectionMW = 150.0;
    e.thermal.remainingSafeHeatStorageMJ = 100.0 * 80.0;
    a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Emergency &&
            !a.continuousThermalSustainable,
            "finite coolant/thermal margin was treated as perpetual");
    e.thermal.remainingSafeHeatStorageMJ = 100.0 * 79.0;
    a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Inoperable,
            "ship claimed reachability after thermal limit");
}

void testFailedEssentialPowerAndMissingReverseManeuver()
{
    auto e = nominal();
    e.power.continuouslyAvailableMW = 100.0;
    e.power.usableStoredEnergyMJ = 1.0e9;
    require(assessShipMobility(e).mobility == MobilityClass::Inoperable,
            "battery energy masked missing peak power output capability");
    // When its discharge circuit can actually meet MW demand, a battery
    // can operate even essential systems for a finite emergency maneuver.
    e.power.maximumStorageDischargeMW = 8300.0;
    require(assessShipMobility(e).mobility == MobilityClass::Emergency,
            "physically deliverable temporary battery power was rejected");
    e = nominal();
    e.control.canBrakeAfterReorientation = false;
    require(assessShipMobility(e).mobility == MobilityClass::Inoperable,
            "motor thrust without eventual braking authorized flight");
    e = nominal();
    e.control.canCompleteHalfTurn = false;
    require(assessShipMobility(e).mobility == MobilityClass::Inoperable,
            "unreachable 180-degree attitude accepted as navigable");
}

void testInvalidMeasureIsUnknown()
{
    auto e = nominal();
    e.thermal.remainingSafeHeatStorageMJ =
        std::numeric_limits<double>::quiet_NaN();
    const auto a = assessShipMobility(e);
    require(a.mobility == MobilityClass::Unverified,
            "nonfinite subsystem measurement was treated as a hard capability");
}

} // namespace

int main()
{
    try
    {
        testUnknownResourceDataCannotAuthorizeFlight();
        testFactoryAndLimited();
        testSingleEngineProofMayAuthorizeOnlyEmergency();
        testTransientElectricAndCoolingBudget();
        testFailedEssentialPowerAndMissingReverseManeuver();
        testInvalidMeasureIsUnknown();
        std::cout << "SHIP MOBILITY ASSESSMENT: PASS\n";
        return EXIT_SUCCESS;
    }
    catch(const std::exception& e)
    {
        std::cerr << "SHIP MOBILITY ASSESSMENT: FAIL: "
                  << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
