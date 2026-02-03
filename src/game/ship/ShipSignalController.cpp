#include "ShipSignalController.h"

#include <iostream>



// //    ##                ##       ##
// //                               ##
// //   ###     #####     ###      #####
// //    ##     ##  ##     ##       ##
// //    ##     ##  ##     ##       ##
// //    ##     ##  ##     ##       ## ##
// //   ####    ##  ##    ####       ###

void ShipSignalController::init(WorldSignal& sig)
{
    // // Теперь можем обращаться к дескриптору через owner



    activeSignal = sig.type;
    // ActiveLabel = sig.owner->desc->name;
    signalControllTimer = 0.0f;
}




// //    ##                                  ##                                 ###
// //                                                                            ##
// //   ###      #####             #####    ###      ### ##  #####     ####      ##      #####
// //    ##     ##                ##         ##     ##  ##   ##  ##       ##     ##     ##
// //    ##      #####             #####     ##     ##  ##   ##  ##    #####     ##      #####
// //    ##          ##                ##    ##      #####   ##  ##   ##  ##     ##          ##
// //   ####    ######            ######    ####        ##   ##  ##    #####    ####    ######
// //                                               #####
// bool ShipSignalController::isSignalAllowed(SignalType type) const
// {
//     return std::find(
//         profile->availableSignals.begin(),
//         profile->availableSignals.end(),
//         type
//     ) != profile->availableSignals.end();
// }



// //                                                          ##
// //                                                          ##
// //  ######    ####     ######  ##  ##    ####     #####    #####
// //   ##  ##  ##  ##   ##  ##   ##  ##   ##  ##   ##         ##
// //   ##      ######   ##  ##   ##  ##   ######    #####     ##
// //   ##      ##        #####   ##  ##   ##            ##    ## ##
// //  ####      #####       ##    ######   #####   ######      ###
// //                       ####
void ShipSignalController::requestSignal(SignalType type)
{
    if (type == activeSignal)
        return;

    activeSignal = type;
    

    signalControllTimer = 0.0f;
}





// //     ###     ##                       ###       ###
// //      ##                               ##        ##
// //      ##    ###      #####    ####     ##        ##      ####
// //   #####     ##     ##           ##    #####     ##     ##  ##
// //  ##  ##     ##      #####    #####    ##  ##    ##     ######
// //  ##  ##     ##          ##  ##  ##    ##  ##    ##     ##
// //   ######   ####    ######    #####   ######    ####     #####

void ShipSignalController::disableSignal()
{
    activeSignal = SignalType::None;
    
}






// //                       ###              ##
// //                        ##              ##
// //  ##  ##   ######       ##    ####     #####    ####
// //  ##  ##    ##  ##   #####       ##     ##     ##  ##
// //  ##  ##    ##  ##  ##  ##    #####     ##     ######
// //  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
// //   ######   ##       ######   #####      ###    #####
// //           ####
void ShipSignalController::update(float dt, WorldSignal& sig)
{

    if (activeSignal == SignalType::None)
    {
        sig.enabled = false;
        sig.type = SignalType::None;
        return;
    }

    // 

    signalControllTimer += dt;

    // пока без реальной модуляции
    sig.enabled = true;
    sig.type = activeSignal;
    
}





    // std::cout
    // << "[ShipSignalController::update] inside: "
    // << "enabled=" << sig.enabled
    // << " SignalType=" << static_cast<int>(sig.type)
    // << " activeSignal=" << static_cast<int>(activeSignal)
    // << std::endl;