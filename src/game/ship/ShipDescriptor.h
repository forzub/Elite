// ShipDescriptor - паспорт - что это за корабль
// не меняется во время игры
// может грузиться из C++ / JSON позже
// один дескриптор → много экземпляров

#pragma once

#include <string>
#include <optional>

#include "game/ship/core/ShipParams.h"
#include "game/ship/ShipHudProfile.h"
#include "game/ship/ShipTypeId.h"

#include "game/ship/cockpit/CockpitContours.h"

#include "src/world/types/SignalType.h"


#include "game/equipment/jammer/JammerDesc.h"
#include "game/equipment/decryptor/DecryptorDesc.h"
#include "game/equipment/signalNode/ReceiverDesc.h"
#include "game/equipment/signalNode/SignalTransmitterDesc.h"
#include "game/equipment/radar/RadarDesc.h"
#include "src/game/equipment/types/RadarMountPoint.h"




struct ShipIdentity
{
    std::string         shipType;      // "elite_cobra_mk1" 
    std::string         shipName;       // "Джерайя" (HUD / меню)
    
};


struct EquipmentPresets
{
    std::optional<DecryptorDesc>                decryptor;
    std::optional<JammerDesc>                   jammer;
    std::optional<game::ReceiverDesc>           receiver;
    std::optional<SignalTransmitterDesc>        transmitter;
    std::optional<game::RadarDesc>                    radar;
};


struct CoreConfig
{
    double reactorMaxOutputMW;     // максимальная мощность реактора
    double thermalMass;            // тепловая инерция корабля
    double maxSafeTemperature;     // безопасная температура
    double baseCoolingRate;        // базовое охлаждение (радиаторы)
};



// Equipment / Systems slots
struct ShipSystemSlots
{
    int reactorSlots                = 0;  // энергогенераторы
    int engineSlots                 = 0;  // двигатели
    int radarSlots                  = 0;
    int weaponSlots                 = 0;
    int decryptorSlots              = 0;
    int jammerSlots                 = 0;
    int dockingComputerSlots        = 0;  // стыковочный компьютер
    int receiverSlots               = 0;  
    int transmitterSlots            = 0;  
    int utilitySlots                = 0;  // сборщик мусора, нанокиты, спецсистемы
    int fuelScopSlots               = 0;  // заборщик топлива
    int tractorBeamSlots            = 0;  // буксировочный луч   

};

// Dock / Craft
struct ShipDockSlots
{
    int fighterSlots        = 0;
    int shuttleSlots        = 0;
    int droneSlots          = 0;
};

// Cargo / Storage
struct ShipStorageCaps
{
    int cargoMass           = 0;
    int cargoVolume         = 0;
    int missileCapacity     = 0;
};

// Survival / Emergency
struct ShipSurvivalCaps
{
    bool hasEscapePod       = false;
    bool hasNanokitBay      = false;
};

struct SignalProfile
{
    std::vector<SignalType> supportedSignals;
};

struct CockpitData
    {
        bool enabled = false;

        CockpitGeometry geometry;
        std::string baseTexturePath;
        std::string glassTexturePath;
    };

struct ReactorDescriptor
{
    double nominalPowerMW;      // 2500 — пик КПД
    double peakPowerMW;         // 3200 — макс эл.
    

    // double heatFactor;       
    
    double optimalTempK;        // 3500 — температура ядра, при которой КПД макс
    double minTempK;            // 2500 — мин. рабочая температура
    double maxTempK;            // 4500 — макс. до разрушения
    
    
    double temperature;          
    double thermalMass;          
    
    double efficiencyMinTemp;           // 0.35 — мин КПД (при мин температуре)
    double efficiencyPeak;              // 0.45 — макс КПД
    double efficiencyMaxTemp;           // 0.40 — мин КПД (при макс температуре)          
};

struct CoolingDescriptor
{
    double radiatorArea;        // м² (1600 для Cobra)
    int panelCount;             // количество панелей (160)
    double emissivity;          // 0.85
    double maxTransferPower;    // МВт (90)
    double nominalPower;        // МВт (2)
    double peakPower;           // МВт (4)
    
};

struct ThermalDescriptor
{
    double thermalMassMJperK    = 850.0;   // теплоемкость корпуса
    double initialTempK         = 293.0;
};


    struct ShipDescriptor
{
    ShipTypeId          typeId;
    ShipIdentity        identity;
    ShipParams          physics;
    ShipHudProfile      hud;

    ShipSystemSlots     systems;
    ShipDockSlots       docks;
    ShipStorageCaps     storage;
    ShipSurvivalCaps    survival;
    SignalProfile       signalProfile;
    

    // НОВОЕ: пресеты оборудования по умолчанию

    
    ReactorDescriptor               reactor;
    CoolingDescriptor               cooling;
    ThermalDescriptor               thermal;
    EquipmentPresets                defaultEquipment;
    std::optional<CockpitData>      cockpit;

    double                          radarCrossSection = 1.0; // коэфициент отражающей поверхности корпуса
    double                          radarSignatureModifier = 1.0; // 1.0 = без изменений для Stealth-эффект через RCS
    RadarMountPoint                 radarMount;
};
