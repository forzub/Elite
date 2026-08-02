#pragma once

#include <memory>
#include <string>

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
class LocalGameHost
{
public:
    LocalGameHost();
    ~LocalGameHost();

    LocalGameHost(const LocalGameHost&) = delete;
    LocalGameHost& operator=(const LocalGameHost&) = delete;

    EntityId playerId() const;

    ITransport& transport();
    const ITransport& transport() const;

    server::ServerAdvanceResult advance(double elapsedSeconds);
    double fixedStepSeconds() const;

    // Local-only authoring/debug seam. Normal gameplay still uses transport.
    void configureWorld(float linearDrag, float maxSafeDecel);

    const SimulationSnapshot& debugSnapshot() const;
    void debugRefreshSnapshot();
    bool debugDestroyShipModule(EntityId shipId, const std::string& moduleId);
    bool debugRestoreShipModule(EntityId shipId, const std::string& moduleId);
    bool debugResetShipStructure(EntityId shipId);
    void debugResetAllShipStructures();
    bool debugDetachShipModule(EntityId shipId, const std::string& moduleId);
    bool debugHangShipModule(EntityId shipId, const std::string& moduleId);
    bool debugReevaluateShipStructure(EntityId shipId);
    bool debugSetShipStructuralLinkHealth(
        EntityId shipId,
        const std::string& linkId,
        float health,
        bool destroyed
    );

    bool debugFastUniverseTime() const;
    void setDebugUniverseTimeSimulation(bool enabled, double timeScale);


private:
    std::unique_ptr<GameServer> m_server;
    std::unique_ptr<LocalLoopbackTransport> m_transport;
    std::unique_ptr<server::ServerRunner> m_runner;
};
}
