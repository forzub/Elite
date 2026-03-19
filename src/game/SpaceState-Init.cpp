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


#include "game/damage/DamageTestObject.h"
#include "game/damage/DamageSystem.h"



using namespace game::damage;
using namespace game::debug;


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
        [&](const glm::mat4& view, const glm::mat4& proj)
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
                m_playerId,
                view,
                proj,
                1,
                "secondCam"
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



    initServerAndClient();


    
}





void SpaceState::initServerAndClient()
{
    
    
    // m_debugServer = std::make_unique<game::debug::DebugServer>();
    // m_debugServer->start(8080);

    m_coreStatusServer = std::make_unique<game::debug::DebugServer>();
    m_coreStatusServer->start(8080, "core_status", 
        { "D:/__elite/work/src/WebSocket/debug/shipcore_viewer.html" });
    
    // Сервис 2: Frustum Debug (WebSocket + HTML)
    m_frustumDebugServer = std::make_unique<game::debug::DebugServer>();
    m_frustumDebugServer->start(8081, "frustum_debug", 
        {"D:/__elite/work/src/WebSocket/debug/frustum_viewer.html"});


    // ============================================================
    // Колбэки для Core Status
    // ============================================================
    
    m_coreStatusServer->onDamageRadiator([this](int index) {
        std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
        ClientShipCommand cmd;
        cmd.type = ClientShipCommand::DamageRadiator;
        cmd.index = index;
        cmd.amount = 0.3;
        m_debugCommands.push_back(cmd);
    });

    m_coreStatusServer->onRepairAllPanels([this]() {
        std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
        ClientShipCommand cmd;
        cmd.type = ClientShipCommand::RepairAllPanels;
        m_debugCommands.push_back(cmd);
    });
    
    // ============================================================
    // Колбэк для Frustum Debug данных
    // ============================================================
    m_sceneRenderer.setDebugCallback([this](const DebugFrameData& data)
    {
        if(m_frustumDebugServer)
        {
            json j = data.toJson();
            j["type"] = "frustum_debug";
            m_frustumDebugServer->broadcastState(j.dump());
        }
    });
    


}




json SpaceState::shipCoreStatusToJson(const game::ShipCoreStatus& status)
{
    json j;
    
    // ----- Реактор -----
    j["reactor"]["temperature"] = status.reactor.temperature;
    j["reactor"]["criticalTemp"] = status.reactor.criticalTemp;
    j["reactor"]["output"] = status.reactor.outputMW;
    j["reactor"]["maxOutput"] = status.reactor.maxOutputMW;
    j["reactor"]["throttle"] = status.reactor.throttle;
    j["reactor"]["instability"] = status.reactor.instability;
    
    const char* statusNames[] = {"Normal", "Overheating", "Critical", "Shutdown"};
    j["reactor"]["status"] = statusNames[status.reactor.status];
    j["reactor"]["integrity"] = status.reactor.integrity;

    j["reactor"]["heatMJ"] = status.reactor.generatedHeat;
    j["reactor"]["heatMW"] = status.reactor.heatGenerationMW;
    
    // ----- Thermal -----
    j["thermal"]["temperature"] = status.thermal.temperature;
    j["thermal"]["thermalMass"] = status.thermal.thermalMass;
    j["thermal"]["storedHeat"] = status.thermal.storedHeat;
    j["thermal"]["heatVolume"] = status.thermal.heatVolume;
    j["thermal"]["criticalTemp"] = status.thermal.thermalCriticalTemp;
    
    // ----- Cooling (только то, что есть) -----
    j["cooling"]["coolantTemp"] = status.cooling.coolantTemp;
    j["cooling"]["totalEfficiency"] = status.cooling.totalEfficiency;
    j["cooling"]["allocatedPower"] = status.cooling.allocatedPowerMW;
    j["cooling"]["requestedPower"] = status.cooling.requestedPowerMW;
    j["cooling"]["radiatedPower"] = status.cooling.radiatedPowerMW;
    j["cooling"]["pumpCapacity"] = status.cooling.pumpCapacity;
    j["cooling"]["pumpHeatMJ"] = status.cooling.pumpHeatMJ;
    j["cooling"]["dt"] = status.cooling.dt;
    
    // Панели
    j["cooling"]["panels"]["count"] = status.cooling.panels.size();
    j["cooling"]["panels"]["damagedCount"] = status.cooling.damagedPanelCount;
    j["cooling"]["panels"]["criticalCount"] = status.cooling.criticalPanelCount;
    
    // Сетка для визуализации
    for (const auto& panel : status.cooling.panels) {
        j["cooling"]["panels"]["grid"].push_back(panel.health);
    }
    
    // ----- PowerBus -----
    j["powerBus"]["available"] = status.powerBus.availablePowerMW;
    j["powerBus"]["overloaded"] = status.powerBus.overloaded;
    j["powerBus"]["totalRequested"] = status.powerBus.totalRequestedMW;

    
    // Потребители
    for (const auto& consumer : status.powerBus.consumers) {
        json c;
        c["name"] = consumer.name;
        c["requested"] = consumer.requestedPowerMW;
        c["allocated"] = consumer.allocatedPowerMW;
        c["priority"] = consumer.priority;
        c["operational"] = consumer.operational;
        c["heatTransfer"] = consumer.heatTransfer;
        j["powerBus"]["consumers"].push_back(c);
    }
    
    // ----- Алерты -----
    for (const auto& alert : status.alerts) {
        json a;
        a["severity"] = alert.severity;
        a["system"] = alert.system;
        a["message"] = alert.message;
        a["value"] = alert.value;
        a["threshold"] = alert.threshold;
        j["alerts"].push_back(a);
    }
    
    j["warningSystems"] = status.warningSystems;
    j["criticalSystems"] = status.criticalSystems;
    j["timestamp"] = std::time(nullptr);
    
    return j;
}


// SpaceState::

void SpaceState::testDamageSystem()
{
    // DamageTestObject obj;

    // TestDamageHandler handler;

    // obj.handler = &handler;

    // DamageEvent event;

    // event.type = DamageType::Laser;
    // event.energy = 10.0;

    // event.position = {1,2,3};
    // event.direction = {0,0,-1};

    // DamageSystem::applyDamage(obj, event);

    // event.position = {2,2,2};
    // DamageSystem::applyDamage(obj, event);

    // event.position = {100,0,0};
    // DamageSystem::applyDamage(obj, event);

    // event.position = {0,0,-2};
    // DamageSystem::applyDamage(obj, event);

    // event.position = {0,0,-4};
    // DamageSystem::applyDamage(obj, event);
}