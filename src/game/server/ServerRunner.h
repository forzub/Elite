#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "src/game/network/SessionMessage.h"
#include "src/game/server/ReplicationInterestPolicy.h"
#include "src/game/server/ReplicationPublicationPolicy.h"

class GameServer;
class IServerTransport;

namespace game::server
{
struct ServerTickPolicy
{
    double fixedStepSeconds = 0.02;
    double maxFrameDeltaSeconds = 0.25;
    double maxAccumulatedDebtSeconds = 0.50;
    std::uint32_t maxCatchUpStepsPerAdvance = 8;
};

struct ServerTransportBinding
{
    IServerTransport* transport = nullptr;
    game::network::ServerSessionId sessionId {};
    std::uint64_t lastPublishedServerTick = 0;

    // Server-only transport demand. Never serialize this plan to the client.
    game::server::ShipReplicationInterestPlan lastShipInterestPlan;

    // Per-destination hydration/cadence/lifecycle memory. It belongs to the
    // transport session, not to authoritative world simulation.
    game::server::ReplicationPublicationState replicationPublicationState;
};

struct ServerAdvanceResult
{
    std::uint32_t stepsExecuted = 0;
    double acceptedElapsedSeconds = 0.0;
    double simulatedSeconds = 0.0;
    double remainingDebtSeconds = 0.0;
    double discardedSeconds = 0.0;
    double totalDiscardedSeconds = 0.0;
    bool catchUpLimited = false;

    // Filled by ServerWorker around the authoritative runtime call. The runner
    // itself stays deterministic and wall-clock agnostic.
    double executionWallSeconds = 0.0;
};

/*
    Owns the fixed-step lifecycle of the local authoritative server.

    The runner is synchronous inside the authoritative server thread. Its only
    external runtime dependency is the server-side transport endpoint; it never
    sees the client transport interface or client state. ServerWorker owns any
    cross-thread pacing/overlap policy around this deterministic fixed-step core.
*/
class ServerRunner
{
public:
    ServerRunner(
        GameServer& server,
        IServerTransport& transport,
        game::network::ServerSessionId sessionId,
        ServerTickPolicy policy = {}
    );

    ServerAdvanceResult advance(double elapsedSeconds);

    // Attach/detach are called on the authoritative server execution context.
    // A concrete socket/file-descriptor handle belongs inside IServerTransport;
    // the runner only binds a platform-neutral endpoint object to server-owned
    // session authority.
    bool attachTransport(
        IServerTransport& transport,
        game::network::ServerSessionId sessionId
    );
    bool detachTransport(game::network::ServerSessionId sessionId);
    std::size_t transportCount() const noexcept;

    bool seedTransportReplicationBaseline(
        game::network::ServerSessionId sessionId,
        const SimulationSnapshot& bootstrapSnapshot
    );

    void resetTiming();

    double fixedStepSeconds() const
    {
        return m_policy.fixedStepSeconds;
    }

    double accumulatedDebtSeconds() const
    {
        return m_accumulatorSeconds;
    }

    double totalDiscardedSeconds() const
    {
        return m_totalDiscardedSeconds;
    }

    const ServerTickPolicy& policy() const
    {
        return m_policy;
    }

private:
    void runFixedStep();
    void receiveInboundMessages(ServerTransportBinding& binding);
    void publishOutboundMessages();
    ServerTransportBinding* findBinding(
        game::network::ServerSessionId sessionId
    ) noexcept;

    GameServer& m_server;
    std::vector<ServerTransportBinding> m_transports;
    ServerTickPolicy m_policy;

    double m_accumulatorSeconds = 0.0;
    double m_totalDiscardedSeconds = 0.0;
};
}
