// SignalController отвечает ТОЛЬКО за:
//  желаемый SignalType
//  проверку «можно / нельзя»
//  выбор SignalPattern
//  управление enabled / patternTime
// Он НЕ:
//  считает дальность
//  не знает про приёмник
//  не знает про мир

// Как Input / мышь подключаются

// Где угодно:

// if (input.keyPressed(KEY_T))
//     signalController.requestSignal(SignalType::Transponder);

// if (input.keyPressed(KEY_S))
//     signalController.requestSignal(SignalType::SOS);

// if (input.keyPressed(KEY_X))
//     signalController.disableSignal();


#pragma once

#include "world/types/SignalType.h"
#include "world/WorldSignal.h"
#include "src/game/signals/SignalPattern.h"
#include "ShipSignalProfile.h"

class ShipSignalController
{
public:
    void init(const ShipSignalProfile& profile);

    // управление извне (Input / AI / UI)
    void requestSignal(SignalType type);
    void disableSignal();

    // вызывается каждый кадр
    void update(float dt, WorldSignal& outSignal);

private:
    const ShipSignalProfile*        profile = nullptr;

    SignalType                      requestedSignal = SignalType::None;
    SignalType                      activeSignal    = SignalType::None;

    const SignalPattern*            currentPattern = nullptr;
    float                           patternTime = 0.0f;

    bool                            isSignalAllowed(SignalType type) const;
};

