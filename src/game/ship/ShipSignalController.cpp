#include "ShipSignalController.h"
#include <algorithm>

#include <iostream>


//    ##                ##       ##
//                               ##
//   ###     #####     ###      #####
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ## ##
//   ####    ##  ##    ####       ###

void ShipSignalController::init(const ShipSignalProfile& p)
{
    profile = &p;
}




//    ##                                  ##                                 ###
//                                                                            ##
//   ###      #####             #####    ###      ### ##  #####     ####      ##      #####
//    ##     ##                ##         ##     ##  ##   ##  ##       ##     ##     ##
//    ##      #####             #####     ##     ##  ##   ##  ##    #####     ##      #####
//    ##          ##                ##    ##      #####   ##  ##   ##  ##     ##          ##
//   ####    ######            ######    ####        ##   ##  ##    #####    ####    ######
//                                               #####
bool ShipSignalController::isSignalAllowed(SignalType type) const
{
    return std::find(
        profile->availableSignals.begin(),
        profile->availableSignals.end(),
        type
    ) != profile->availableSignals.end();
}



//                                                          ##
//                                                          ##
//  ######    ####     ######  ##  ##    ####     #####    #####
//   ##  ##  ##  ##   ##  ##   ##  ##   ##  ##   ##         ##
//   ##      ######   ##  ##   ##  ##   ######    #####     ##
//   ##      ##        #####   ##  ##   ##            ##    ## ##
//  ####      #####       ##    ######   #####   ######      ###
//                       ####
void ShipSignalController::requestSignal(SignalType type)
{
    if (type == SignalType::None || isSignalAllowed(type))
        requestedSignal = type;
}





//     ###     ##                       ###       ###
//      ##                               ##        ##
//      ##    ###      #####    ####     ##        ##      ####
//   #####     ##     ##           ##    #####     ##     ##  ##
//  ##  ##     ##      #####    #####    ##  ##    ##     ######
//  ##  ##     ##          ##  ##  ##    ##  ##    ##     ##
//   ######   ####    ######    #####   ######    ####     #####

void ShipSignalController::disableSignal()
{
    requestedSignal = SignalType::None;
}






//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####
void ShipSignalController::update(float dt, WorldSignal& sig)
{

    
    if (requestedSignal != activeSignal)
    {
        activeSignal = requestedSignal;
        patternTime = 0.0f;

        if (activeSignal != SignalType::None)
            currentPattern = profile->getPattern(activeSignal);
        else
            currentPattern = nullptr;
    }

    // if (activeSignal == SignalType::None || !currentPattern)
    if (activeSignal == SignalType::None)
    {
        sig.enabled = false;
        return;
    }

    patternTime += dt;
    // bool phaseActive = currentPattern->isActive(patternTime);
    // sig.enabled = phaseActive;

    sig.type    = activeSignal;
    
    
    sig.enabled = true;
    // sig.enabled = true;
    // sig.pattern = currentPattern;
}
