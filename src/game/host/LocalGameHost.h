#pragma once

#include <memory>
#include <string>

#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/server/ServerRunner.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/scene/EntityID.h"
#include "src/world/WorldParams.h"

class GameServer;
class ITransport;
class LocalLoopbackTransport;

namespace game::host
{
/*
    Owns the complete in-process authoritative runtime.

    Client-facing states receive only the transport endpoint. Local debug and
    authoring operations are exposed as a narrow host facade; GameServer itself
    never leaks into client-facing state types.
*/
class LocalGameHost final : public game::debug::IDebugSessionControl
{
public:
    explicit LocalGameHost(const WorldParams& worldParams);
    ~LocalGameHost();

    LocalGameHost(const LocalGameHost&) = delete;
    LocalGameHost& operator=(const LocalGameHost&) = delete;

    EntityId playerId() const;

    ITransport& transport();
    const ITransport& transport() const;

    server::ServerAdvanceResult advance(double elapsedSeconds);
    double fixedStepSeconds() const;


    const SimulationSnapshot& snapshot() const override;
    void refreshSnapshot() override;
    bool destroyShipModule(EntityId shipId, const std::string& moduleId) override;
    bool restoreShipModule(EntityId shipId, const std::string& moduleId) override;
    bool resetShipStructure(EntityId shipId) override;
    void resetAllShipStructures() override;
    bool detachShipModule(EntityId shipId, const std::string& moduleId) override;
    bool hangShipModule(EntityId shipId, const std::string& moduleId) override;
    bool reevaluateShipStructure(EntityId shipId) override;
    bool setShipStructuralLinkHealth(
        EntityId shipId,
        const std::string& linkId,
        float health,
        bool destroyed
    ) override;

    bool fastUniverseTime() const override;
    bool universeTimeSimulation() const override;
    double universeTimeScale() const override;
    double configuredUniverseTimeScale() const override;
    void setUniverseTimeSimulation(bool enabled, double timeScale) override;


private:
    std::unique_ptr<GameServer> m_server;
    std::unique_ptr<LocalLoopbackTransport> m_transport;
    std::unique_ptr<server::ServerRunner> m_runner;
};
}
