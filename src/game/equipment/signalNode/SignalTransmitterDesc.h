#pragma once

#include "src/game/equipment/EquipmentDescriptor.h"
#include "world/types/SignalType.h"
#include "world/types/SignalDisplayClass.h"
#include <vector>

struct SignalTransmitterDesc
{
    EquipmentDescriptor base;

    // baseRange — расстояние, на котором
    // при txPower = 1.0 и sensitivity = 1.0
    // receivedPower == receiverBaseNoise

    // baseRange — расстояние, на котором сигнал при идеальных условиях
    // (без помех, без глушения, чувствительность = 1.0)
    // переходит из “уверенно принимается” в “на границе декодирования”

    // При   baseRange  50'000.0f,      // м txPower = 1.0:

    // Дистанция	    SNR	            Состояние
    // 50 км	          1	              граница слышимости
    // 29 км	         ≈3	              декодирование
    // 16 км	         ≈10	          уверенно


    float                       txPower;
    float                       baseRange;

    std::vector<SignalType>     modes;
    SignalDisplayClass          displayClass;
};