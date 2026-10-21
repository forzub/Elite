#include "src/game/ship/computer/MotherComputer.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
using game::ship::computer::MotherComputer;
using game::ship::computer::MotherComputerInput;
using game::ship::capability::MobilityClass;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

MotherComputerInput makeInput(std::uint64_t shipId,
                              std::uint64_t tick)
{
    MotherComputerInput input;
    input.shipInstanceId = shipId;
    input.serverTick = tick;
    input.physicalControlModel =
        static_cast<int>(game::ship::plant::PhysicalControlModelId::AssistedI);
    input.physicalEnvelopeValid = true;
    input.physicalEnvelope.maxGs = 5.0f;
    input.physicalEnvelope.maxLinearGs = 5.0f;
    input.physicalEnvelope.angularAccel = 3.0f;
    input.physicalEnvelope.strafeAccel = 20.0f;
    input.physicalEnvelope.massKg = 260000.0;
    input.physicalEnvelope.forwardMainEngineAvailable = true;
    input.physicalEnvelope.reverseMainEngineAvailable = true;
    input.physicalEnvelope.forwardMainEngineAccelerationMps2 = 30.0f;
    input.physicalEnvelope.reverseMainEngineAccelerationMps2 = 30.0f;
    input.installedMotorAuthority.valid = true;
    input.installedMotorAuthority.model =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    input.installedMotorAuthority.positive = {
        10.0,10.0,20.0,1.0,1.0,1.0
    };
    input.installedMotorAuthority.negative = {
        10.0,10.0,20.0,1.0,1.0,1.0
    };
    input.reactorOutputMW = 45.0;
    input.busRequestedMW = 35.0;
    input.thermalTemperatureK = 700.0;
    input.thermalCriticalK = 1200.0;
    input.biteCriticalSystems = 1;
    input.biteAndSystems.criticalSystems.push_back("engine");
    input.installedEquipmentBite.sourceRevision = tick;
    input.installedEquipmentBite.summary.failedCount = 1;
    game::bite::Fault fault;
    fault.equipmentId = "aft-left";
    fault.severity = game::bite::Severity::Failed;
    input.installedEquipmentBite.faults.push_back(fault);
    return input;
}

void testImmediateAuthorityDoesNotWaitForWorker()
{
    MotherComputer computer;
    ShipParams output {};
    game::ship::telemetry::ShipControlAuthorityTelemetry authority {};

    require(!computer.assistedFlightEnvelope(19,1,output,authority),
            "never-observed computer invented authority");
    computer.observe(makeInput(19,1));
    require(computer.assistedFlightEnvelope(19,1,output,authority),
            "current motor authority was blocked by advisory thread");
    require(output.forwardMainEngineAccelerationMps2 <= 20.01f &&
            output.forwardMainEngineAccelerationMps2 >= 19.99f,
            "computer silently replaced installed motor limits with rating");
    require(!computer.assistedFlightEnvelope(19,2,output,authority),
            "older authority was returned to a newer server tick");
    require(!computer.assistedFlightEnvelope(20,1,output,authority),
            "authority from a different ship was returned");

    auto latest=computer.snapshot();
    require(latest.valid &&
            latest.input.serverTick == 1 &&
            latest.input.biteCriticalSystems == 1 &&
            latest.input.biteAndSystems.criticalSystems.size() == 1 &&
            latest.input.installedEquipmentBite.sourceRevision == 1 &&
            latest.input.installedEquipmentBite.faults.size() == 1,
            "BITE and source revision were not retained in cockpit snapshot");
}

void testLatestWinsAndInvalidDoesNotPromiseFlight()
{
    MotherComputer computer;
    for(std::uint64_t i=1;i<=150;++i)
    {
        auto input=makeInput(9,i);
        input.busRequestedMW=100.0+static_cast<double>(i);
        computer.observe(input);
    }
    auto latest=computer.snapshot();
    require(latest.valid && latest.input.serverTick==150 &&
            latest.input.busRequestedMW==250.0,
            "the asynchronous mailbox failed latest-wins ordering");
    computer.observe(makeInput(9,149));
    require(computer.snapshot().input.serverTick==150,
            "a delayed server snapshot rewound flight authority");

    bool completed=false;
    for(int i=0;i<300;++i)
    {
        latest=computer.snapshot();
        if(latest.analysisCurrent)
        {
            completed=true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(completed,
            "worker failed to publish latest generation advisory");
    require(latest.mobility.mobility==MobilityClass::Unverified &&
            !latest.mobility.candidateForSimpleReturnRoute,
            "power/thermal and 6DOF proof gaps were misreported as flyable");
    require(latest.autopilotAuthorityCurrent,
            "async advisory mistakenly disabled the current flight lane");
    require(latest.advisory.installedMotorBasisKnown &&
            latest.advisory.oneAxisForward &&
            latest.advisory.oneAxisReverse &&
            latest.advisory.yawBidirectional &&
            latest.advisory.pitchBidirectional &&
            latest.advisory.rollBidirectional &&
            latest.advisory.optimisticHalfTurnSeconds > 0.0 &&
            latest.advisory.motorPowerAllocationProofMissing &&
            latest.advisory.thermalEnduranceProofMissing,
            "advisory did not separate signed motor preview from proof");

    auto replacement=makeInput(10,1);
    replacement.installedMotorAuthority.valid=false;
    computer.observe(replacement);
    ShipParams effective {};
    game::ship::telemetry::ShipControlAuthorityTelemetry authority {};
    require(!computer.assistedFlightEnvelope(10,1,effective,authority),
            "new ship inherited healthy motors from previous ship");
    computer.stop();
}

void testSameTickNewGenerationCannotPublishObsoleteWork()
{
    MotherComputer computer;
    auto first=makeInput(22,88);
    first.biteCriticalSystems=5;
    auto second=makeInput(22,88);
    second.biteCriticalSystems=0;
    computer.observe(first);
    computer.observe(second);
    bool complete=false;
    for(int i=0;i<300;++i)
    {
        auto current=computer.snapshot();
        if(current.analysisCurrent)
        {
            require(current.input.biteCriticalSystems==0,
                    "older same-tick worker published over newer BITE");
            complete=true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(complete, "same-tick generation was never completed");
}

} // namespace

int main()
{
    try
    {
        testImmediateAuthorityDoesNotWaitForWorker();
        testLatestWinsAndInvalidDoesNotPromiseFlight();
        testSameTickNewGenerationCannotPublishObsoleteWork();
        std::cout << "MOTHER COMPUTER: PASS\n";
        return EXIT_SUCCESS;
    }
    catch(const std::exception& e)
    {
        std::cerr << "MOTHER COMPUTER: FAIL: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
