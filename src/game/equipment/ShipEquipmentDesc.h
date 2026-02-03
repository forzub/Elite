#pragma once

#include "src/game/equipment/jammer/JammerModule.h"
#include "src/game/equipment/signalNode/SignalTransmitterModule.h"
#include "src/game/equipment/signalNode/ReceiverModule.h" 
#include "src/game/equipment/decryptor/DecryptorModule.h"

struct ShipEquipmentDesc
{
    ReceiverDesc                receiver;
    SignalTransmitterDesc       transmitter;
    JammerDesc                  jammer;
    DecryptorModule             decryptor;
};
