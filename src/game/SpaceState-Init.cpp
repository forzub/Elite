#include <glad/gl.h>
#include <iostream>
#include <chrono>
#include <thread>

#include "SpaceState.h"
#include "src/game/RuntimeFeatureFlags.h"
#include "core/StateStack.h"
#include "core/log.h"


#include "render/DebugGrid.h"
#include "src/render/camera/RenderCameraViewport.h"
#include "src/ui/components/UIText.h"
#include "src/core/Application.h"
#include "src/game/session/IGameSession.h"

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
    if (!context().app)
        throw std::runtime_error("SpaceState has no Application context");

    m_session = &context().app->gameSession();
    m_playerId = m_session->playerId();
    m_debugSession = m_session->debugControl();
}


void SpaceState::initClient()
{
    if (!m_session)
        throw std::runtime_error("SpaceState has no active game session");

    m_client = &m_session->client();
}



void SpaceState::initHUD()
{
    if (!m_client)
        throw std::runtime_error("SpaceState has no game client");

    if (!m_client->readyForGameplay())
    {
        const std::string& reason = m_client->connectionError();
        throw std::runtime_error(
            reason.empty()
                ? "Client session is not ready for gameplay"
                : reason
        );
    }

    const Viewport& vp = context().viewport();
    const auto& ships = m_client->world().ships();
    const ClientShipState* playerShip = nullptr;

    const auto playerIt = ships.find(m_playerId.value);
    if (playerIt != ships.end())
    {
        playerShip = &playerIt->second;
    }

    if (!playerShip)
    {
        throw std::runtime_error("Player ship not found in snapshot");
    }

    if (!playerShip->descriptor)
    {
        throw std::runtime_error(
            "Player ship descriptor is missing from client startup state"
        );
    }

    const ShipDescriptor& desc = *playerShip->descriptor;
    ShipTransform initialTransform = playerShip->transform;
    m_playerView = std::make_unique<PlayerShipView>();
    m_playerView->init(context(), &desc, initialTransform);

    m_playerView->setAttachmentOverrides(&m_attachmentEditorOverrides);




    // -------------------  UI контенер -------------------------
    uiRoot = std::make_unique<UIContainer>();

    auto makeTelemetryText =
    [](const std::string& id, glm::vec2 pos) -> std::unique_ptr<UIText>
    {
        auto t = std::make_unique<UIText>();
        t->id = id;
        t->label = "";
        t->fontPath = "assets/fonts/Roboto-Light.ttf";
        t->fontSizeRel = 0.014f;
        t->color = {0.2f, 0.8f, 1.0f, 0.32f};
        t->position = pos;
        return t;
    };

    // ================== задняя камера =========================
    auto rear = std::make_unique<UICameraView>();

    rear->id                    = "rear_camera";
    rear->position              = {0.72f, 0.05f};
    rear->size                  = {0.25f, 0.18f};
    rear->cornerRadiusRel       = 0.12f;
    rear->borderThicknessRel    = 0.008f;
    rear->camera                = &m_playerView->camera(ShipCameraMode::Rear);
    rear->borderColor           = {0.2f, 0.8f, 1.0f};


    SceneRenderPolicy rearPolicy;
    rearPolicy.drawLabels = false;
    rearPolicy.drawDebug = false;

    // Эти значения будут дополнительно уточняться при рендере,
    // но стартовые оставляем безопасными.
    rearPolicy.drawStarfield = true;
    rearPolicy.drawCelestial = true;
    rearPolicy.drawFarStationProxy = true;
    rearPolicy.drawObjects = true;
    rearPolicy.drawVisualShips = true;
    rearPolicy.drawVisualDrones = true;

    // Ограничиваем только декоративные visual-ships,
    // а не станцию, планеты, звёзды и реальные объекты.
    rearPolicy.maxVisualShipsToDraw = 64;

 
    rear->drawCallback =
        [this, rearPolicy](const glm::mat4& view, const glm::mat4& proj)
        {
        SceneRenderPolicy policy = rearPolicy;
        const auto& dbg = debug::get().render;

        policy.drawStarfield = dbg.renderStarfield;
        policy.drawCelestial = dbg.renderCelestialBodies;
        policy.drawFarStationProxy = dbg.renderHubs;
        policy.drawHubs = dbg.renderHubs;
        policy.drawLargeObjects = dbg.renderLargeObjects;
        policy.drawObjects =
            dbg.renderHubs ||
            dbg.renderLargeObjects ||
            dbg.renderCelestialBodies;
        policy.drawRealShips = dbg.renderRealShips;
        policy.drawPlayerShip = dbg.renderPlayerShip;
        policy.drawNpcShips = dbg.renderNpcShips;
        policy.drawVisualShips = dbg.renderVisualShips;
        policy.drawTrafficShips = dbg.renderTrafficShips;
        policy.drawVisualDrones = dbg.renderVisualShips;
             
        
        glm::dvec3 observerGalacticPositionLy {0.0};
        const glm::dvec3* observerGalacticPositionPtr = nullptr;
        if (resolvePlayerGalacticPositionLy(observerGalacticPositionLy))
            observerGalacticPositionPtr = &observerGalacticPositionLy;

        m_sceneRenderer.render(
                m_client->world(),
                m_playerId,
                view,
                proj,
                1,
                "secondCam",
                policy,
                observerGalacticPositionPtr
            );


            m_perfRearStats = m_sceneRenderer.lastStats();
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
    labelMainViewText->fontPath = "assets/fonts/Roboto-Bold.ttf";
    labelMainViewText->fontSizeRel = 0.03f; // относительный размер от высоты экрана 2.5% от высоты
    labelMainViewText->color = {0.2f, 0.8f, 1.0f, 0.1f};
    labelMainViewText->position = {0.06f, 0.13f};

    uiRoot->addChild(std::move(labelMainViewText));


    // ================== Main camera coordinates / speed =========================


    const float mainTelemetryX = 0.06f;
    const float mainTelemetryY = 0.175f;
    const float mainTelemetryStep = 0.020f;

    uiRoot->addChild(makeTelemetryText("main_coord_cell", {mainTelemetryX, mainTelemetryY + mainTelemetryStep * 0.0f}));
    uiRoot->addChild(makeTelemetryText("main_coord_x",    {mainTelemetryX, mainTelemetryY + mainTelemetryStep * 1.0f}));
    uiRoot->addChild(makeTelemetryText("main_coord_y",    {mainTelemetryX, mainTelemetryY + mainTelemetryStep * 2.0f}));
    uiRoot->addChild(makeTelemetryText("main_coord_z",    {mainTelemetryX, mainTelemetryY + mainTelemetryStep * 3.0f}));
    uiRoot->addChild(makeTelemetryText("main_coord_v",    {mainTelemetryX, mainTelemetryY + mainTelemetryStep * 4.0f}));


    // ================== Radar HUD creation =========================

    m_radarWidget = nullptr;

    if constexpr (game::runtime::RadarHudEnabled)
    {
        if (desc.defaultEquipment.radar.has_value())
        {
            const game::RadarDesc& radarDesc =
                desc.defaultEquipment.radar.value();

            auto radar = RadarWidgetFactory::create(
                radarDesc.type,
                radarDesc.visualProfile
            );

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
        }
    }



    // =======================================================================
    // инициализация параметров рендера
    // =======================================================================
    m_hudRenderer.init(context());

    m_worldLabelRenderer.init(context());

    m_flightVectorIndicatorRenderer.init();

    initServerAndClient();


    
}





void SpaceState::initServerAndClient()
{
    
    
    // Browser diagnostics are served by the shared HtmlUiServer.
    // Individual debug pages use panel routing over the same WebSocket.

    // ============================================================
    // Колбэк для Frustum Debug данных
    // ============================================================
    m_sceneRenderer.setDebugCallback([this](const DebugFrameData& data)
    {
        
        
            json j = data.toJson();
            j["type"] = "frustum_debug";
            pushFrustumDebugState(j);
        
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
    
}