#include "transmitters.h"

const SignalTransmitterDesc WSDR_TX13 = {
    .base = {
        "WSDR-TX13",
        "Weyland Signal Division",
        "Стандартный корабельный передатчик общего назначения"
    },
    .txPower   = 10000.0f,        // SNR = 1 на baseRange
    .baseRange = 500'000.0f,   // 50 км
    .displayClass = SignalDisplayClass::Local,
    .powerConsumption = 5.0,
    .heatGeneration   = 0.2
};



const SignalTransmitterDesc ORBITAL_BEACON_TX = {
    .base = {
        "ORB-TX-88",
        "Helios Navigation Systems",
        "Мощный стационарный передатчик навигационных маяков"
    },
    .txPower   = 9.0f,
    .baseRange = 300'000.0f,  // 300 км
    .displayClass = SignalDisplayClass::Global,
    .powerConsumption = 5.0,
    .heatGeneration   = 0.2
};