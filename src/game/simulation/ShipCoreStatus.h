#pragma once
#include <string>
#include <vector>

namespace game
{

struct ReactorStatus {
    double temperature = 0.0;
    double criticalTemp = 0.0;
    double outputMW = 0.0;
    double maxOutputMW = 0.0;
    double throttle = 0.0;
    double instability = 0.0;
    uint8_t status = 0; // 0=Normal,1=Overheating,2=Critical,3=Shutdown
    double integrity = 1.0;
    double generatedHeat = 0.0;         // МДж за шаг
    double heatGenerationMW = 0.0;    // текущий МВт
};

struct ThermalStatus {
    double temperature = 293.0;      // K
    double thermalMass = 100.0;       // МДж/К
    double storedHeat = 0.0;          // МДж (накоплено)
    double heatVolume = 0.0;          // МДж (за последний шаг)
    double thermalCriticalTemp = 1200.0;
};

struct RadiatorPanelStatus {
    double health = 1.0;
    double efficiency = 1.0;
};

struct CoolingStatus {
    double coolantTemp = 293.0;         // K (из ThermalSystem)
    double totalEfficiency = 1.0;
    double allocatedPowerMW = 0.0;      // из getAllocatedPower()
    double requestedPowerMW = 0.0;      // из getRequestedPower()
    double radiatedPowerMW = 0.0;       // из getLastRadiatedPower()
    double pumpCapacity = 0.0;          // переносмая мощность
    double pumpHeatMJ = 0.0;            // переносмая тепло
    double dt = 1/60;
    
    std::vector<RadiatorPanelStatus> panels;
    int damagedPanelCount = 0;
    int criticalPanelCount = 0;
    std::vector<int> failedPanelIndices;
};

struct PowerConsumerStatus {
    std::string name;
    double requestedPowerMW = 0.0;
    double allocatedPowerMW = 0.0;
    int priority = 0; // 0=Critical, 1=Combat, 2=Mobility, 3=Support, 4=Comfort
    bool operational = true;
};

struct PowerBusStatus {
    double availablePowerMW = 0.0;
    bool overloaded = false;
    double totalRequestedMW = 0.0;
    std::vector<PowerConsumerStatus> consumers;
};

struct AlertStatus {
    int severity = 0; // 1=warning, 2=critical
    std::string system;
    std::string message;
    double value = 0.0;
    double threshold = 0.0;
};

struct ShipCoreStatus {
    ReactorStatus reactor;
    ThermalStatus thermal;
    CoolingStatus cooling;
    PowerBusStatus powerBus;
    
    std::vector<AlertStatus> alerts;
    std::vector<std::string> warningSystems;
    std::vector<std::string> criticalSystems;
};

}