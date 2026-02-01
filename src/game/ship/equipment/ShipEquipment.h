#pragma once

#include "SignalEmitterModule.h"
#include "JammerModule.h"
#include "ReceiverModule.h"   // уже есть

struct ShipEquipment
{
    ReceiverModule      receiver;
    SignalEmitterModule transmitter;
    JammerModule        jammer;
};
