#pragma once

#include "signalNode/SignalTransmitterDesc.h"
#include "signalNode/ReceiverDesc.h"
#include "jammer/JammerDesc.h"   // если есть

struct ShipEquipmentDesc
{
    ReceiverDesc          receiver;
    SignalTransmitterDesc transmitter;
    JammerDesc            jammer;
};
