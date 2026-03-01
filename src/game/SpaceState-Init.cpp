#include <glad/gl.h>
#include <iostream>

#include "SpaceState.h"
#include "core/StateStack.h"
#include "core/Log.h"


#include "render/DebugGrid.h"
#include "src/render/camera/RenderCameraViewport.h"
#include "src/ui/components/UIText.h"
#include "src/game/network/LocalLoopbackTransport.h"

#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"

#include "src/game/equipment/radar/RadarDesc.h"
#include "src/game/equipment/radar/IRadarVisualConfig.h"

#include "src/ui/components/radar/RadarWidgetFactory.h"
#include "src/ui/components/radar/RadarWidgetBase.h"





void SpaceState::initServer()
{
    m_server = std::make_unique<GameServer>();
    m_playerId = m_server->playerId();
     
    m_transport = std::make_unique<LocalLoopbackTransport>(
        m_server.get()
    );

    m_server->update(0.0f);
}






void SpaceState::initClient()
{
    m_client = std::make_unique<GameClient>(
        m_transport.get(),
        m_playerId
    );
}






void SpaceState::initHUD()
{
    const Viewport& vp = context().viewport();
    const auto& snap = m_server->snapshot();
    const ShipSnapshot* playerSnap = nullptr;

    for (const auto& s : snap.ships)
    {
        if (s.id == m_playerId)
        {
            playerSnap = &s;
            break;
        }
    }

    if (!playerSnap)
    {
        throw std::runtime_error("Player ship not found in snapshot");
    }

    const ShipDescriptor& desc =
    ShipDescriptorRegistry::get(playerSnap->typeId);
    ShipTransform initialTransform = playerSnap->transform;
    m_playerView = std::make_unique<PlayerShipView>();
    m_playerView->init(context(), &desc, initialTransform);


    // -------------------  UI контенер -------------------------
    uiRoot = std::make_unique<UIContainer>();

    // ================== задняя камера =========================
    auto rear = std::make_unique<UICameraView>();

    rear->id                    = "rear_camera";
    rear->position              = {0.72f, 0.05f};
    rear->size                  = {0.25f, 0.18f};
    rear->cornerRadiusRel       = 0.12f;
    rear->borderThicknessRel    = 0.008f;
    rear->camera                = &m_playerView->camera(ShipCameraMode::Rear);
    rear->borderColor           = {0.2f, 0.8f, 1.0f};

 
    rear->drawCallback =
        [&](const glm::mat4&, const glm::mat4&)
        {
            // const auto& ships = m_client->world().ships();

            // auto it = ships.find(m_playerId.value);
            // if (it == ships.end())
            //     return;

            // DebugGrid::drawInfinite(
            //     it->second.transform.position,
            //     20000.0f,
            //     100
            // );
             m_sceneRenderer.render(
                m_client->world(),
                m_playerId
            );
        };

    rearView = rear.get();                  // ← сохраняем raw pointer если нужен
    uiRoot->addChild(std::move(rear));      // ← теперь отдаём владение

    auto labelMiniViewText = std::make_unique<UIText>();
    labelMiniViewText->id = "rear_label";
    labelMiniViewText->label = "REAR";
    labelMiniViewText->fontPath = "assets/fonts/Roboto-Light.ttf";
    labelMiniViewText->fontSizeRel = 0.025f; // относительный размер от высоты экрана 2.5% от высоты
    labelMiniViewText->color = {0.2f, 0.8f, 1.0f, 0.5f};
    labelMiniViewText->position = {0.03f, 0.15f};

    

    rearView->addChild(std::move(labelMiniViewText));

    // ================== текст передней камеры =========================

    auto labelMainViewText = std::make_unique<UIText>();
    labelMainViewText->id = "main_label";
    labelMainViewText->label = "FRONT";
    labelMainViewText->fontPath = "assets/fonts/Roboto-BOLD.ttf";
    labelMainViewText->fontSizeRel = 0.03f; // относительный размер от высоты экрана 2.5% от высоты
    labelMainViewText->color = {0.2f, 0.8f, 1.0f, 0.1f};
    labelMainViewText->position = {0.06f, 0.13f};

    uiRoot->addChild(std::move(labelMainViewText));

    // ================== Radar HUD creation =========================

    if (!desc.defaultEquipment.radar.has_value())
        {
            std::cout << "No radar installed in descriptor\n";
            return;
        }
    
    const game::RadarDesc& radarDesc =
                desc.defaultEquipment.radar.value();

    auto radar = RadarWidgetFactory::create(radarDesc.type, radarDesc.visualProfile);
    
    
    
    if (radarDesc.visual)
        radar->applyVisualConfig(*radarDesc.visual);
    
    if (radarDesc.effects) 
        radar->applyEffectsConfig(*radarDesc.effects);
        
     
    radar->id       = "radar";
    radar->position = desc.radarMount.normalizedPosition;
    radar->size     = desc.radarMount.normalizedSize;
    radar->anchor   = UIAnchor::Center;
    
    m_radarWidget = radar.get();
    m_radarWidget->configureFromDesc(
        radarDesc.sweepSpeedDegPerSec,
        radarDesc.maxRange
    );

    uiRoot->addChild(std::move(radar));









    // =======================================================================
    // инициализация параметров рендера
    // =======================================================================
    m_hudRenderer.init(context());
    m_worldLabelRenderer.init(context());

}