// ShipDescriptor - паспорт - что это за корабль
// не меняется во время игры
// может грузиться из C++ / JSON позже
// один дескриптор → много экземпляров

#pragma once

#include <string>
#include <optional>

#include "game/ship/ShipParams.h"
#include "game/ship/ShipHudProfile.h"

#include "src/world/types/SignalType.h"


#include "game/equipment/jammer/JammerDesc.h"
#include "game/equipment/decryptor/DecryptorDesc.h"
#include "game/equipment/signalNode/ReceiverDesc.h"
#include "game/equipment/signalNode/SignalTransmitterDesc.h"


struct ShipIdentity
{
    std::string shipType;      // "elite_cobra_mk1" 
    std::string shipName;       // "Джерайя" (HUD / меню)
};


struct EquipmentPresets
{
    std::optional<DecryptorDesc>                decryptor;
    std::optional<JammerDesc>                   jammer;
    std::optional<ReceiverDesc>                 receiver;
    std::optional<SignalTransmitterDesc>        transmitter;
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


    struct ShipDescriptor
{
    ShipIdentity        identity;
    ShipParams          physics;
    ShipHudProfile      hud;

    ShipSystemSlots     systems;
    ShipDockSlots       docks;
    ShipStorageCaps     storage;
    ShipSurvivalCaps    survival;
    SignalProfile       signalProfile;

    // НОВОЕ: пресеты оборудования по умолчанию
    EquipmentPresets    defaultEquipment;
    
};
