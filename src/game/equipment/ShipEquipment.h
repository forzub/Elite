#pragma once

#include "radar/RadarModule.h"
#include "signalNode/ReceiverModule.h"
#include "signalNode/SignalTransmitterModule.h"

namespace game {

struct ShipEquipment
{
    RadarModule    radar;
    ReceiverModule receiver;
    SignalTransmitterModule transmitter;
};

}