#pragma once

#include <memory>
#include <string>

#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/server/ServerRunner.h"
#include "src/world/WorldParams.h"

class GameServer;
class IServerTransport;

namespace game::server
{
/*
    Sole owner of the in-process authoritative GameServer.

    LocalGameHost owns this runtime, but never reaches through it to GameServer.
    Keeping ownership behind one server-side object is the prerequisite for
    moving the whole runtime to a worker thread without exposing authoritative
    memory to the client/application thread.
*/
class ServerRuntime final : public game::debug::IDebugSessionControl
{
public:
    ServerRuntime(
        const WorldParams& worldParams,
        IServerTransport& transport
    );
    ~ServerRuntime();

    ServerRuntime(const ServerRuntime&) = delete;
    ServerRuntime& operator=(const ServerRuntime&) = delete;

    ServerAdvanceResult advance(double elapsedSeconds);
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
    std::unique_ptr<ServerRunner> m_runner;
};
}
