#pragma once

#include "src/game/equipment/jammer/JammerModule.h"
#include "src/game/equipment/signalNode/SignalTransmitterModule.h"
#include "src/game/equipment/signalNode/ReceiverModule.h" 
#include "src/game/equipment/decryptor/DecryptorModule.h" 

#include "ShipEquipmentDesc.h" 

#include <tuple>

// перечисляем все оборудование, которое может быть установлено в корабле. 
// что из этого точно там будет - описанов в файле корабля.

struct ShipEquipment
{
    ReceiverModule              receiver;
    SignalTransmitterModule     transmitter;
    JammerModule                jammer;
    DecryptorModule             decryptor;


    auto asTuple()
    {
        return std::tie(receiver, transmitter, jammer);
    }
    
};
