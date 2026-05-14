#pragma once
#include <vector>
#include "game/core/QuantitySlot.h"


struct ShipSystemState
{
    QuantitySlot reactor;
    QuantitySlot engine;
    QuantitySlot radar;
    QuantitySlot weapon;
    QuantitySlot decryptor;
    QuantitySlot jammer;
    QuantitySlot dockingComputer;
    QuantitySlot receiver;
    QuantitySlot transmitter;
    QuantitySlot utility;
    QuantitySlot fuelScop;
    QuantitySlot tractorBeam;
};

struct ShipDockState
{
    QuantitySlot fighter;
    QuantitySlot shuttle;
    QuantitySlot drone;
};