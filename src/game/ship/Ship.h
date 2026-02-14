


#pragma once
#include <vector>

#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/view/PlayerShipView.h"
#include "src/game/ship/controller/PlayerInputMapper.h"


// struct CockpitSetup
// {
//     CockpitGeometry geometry;
//     std::string baseTexturePath;
//     std::string glassTexturePath;
// };



// главный класс описания корабля, общий для всех летающих
// описание находится в папке description
// связь с файлом описания через структуру - ShipDecription

struct Ship
{
    // ─────────────────────
    // Агрегированные части
    // ─────────────────────
    ShipCore               core;
    PlayerShipView         view;
    ShipControlState                        control;            // - управление
    // --- состояние ---
    PlayerInputMapper                       inputMapper;
    
    
    // --- lifecycle ---
    void init(
        StateContext&                       context, 
        ShipRole                            role,
        const ShipDescriptor&               descriptor, 
        glm::vec3                           position,
       const ShipInitData&                  initData
    );


    void handleInput();

    void update(
        float dt,
        const WorldParams&                          world,
        const std::vector<WorldSignal>&             worldSignals,
        const std::vector<Planet>&                  planets,
        const std::vector<InterferenceSource>&      interferenceSources
    );


    void updateControlIntent();
    void updateCamera(float dt);

    


    // ───────────────
    // перенос криптокарт из shipInventory в decryptor и обратно
    // ─────────────── 
    bool installCryptoCard(CryptoCard* card);
    bool removeCryptoCard(CryptoCard* card);

    bool addItem(Item* item);
    bool REMOVEItem(Item* item);

    const ShipCockpitState& getCockpitState() const;
};

