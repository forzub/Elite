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





class ShipSignalController
{
public:
    void init(WorldSignal& emmitSig);

    void requestSignal(SignalType type);   // вызывается из input / AI
    void update(float dt, WorldSignal& sig);
    void disableSignal();

private:

    SignalType              activeSignal = SignalType::None;
    float                   signalControllTimer = 0.0f;
    std::string             ActiveLabel;

    
    // const SignalPattern* getPattern(SignalType type) const;
    // const SignalPattern*    currentPattern = nullptr;
    // кастомное переопределения паттернов
    // std::unordered_map<SignalType, const SignalPattern*>    customPatterns;

};
