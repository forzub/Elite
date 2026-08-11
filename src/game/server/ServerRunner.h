#pragma once

#include <cstdint>

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

struct ServerAdvanceResult
{
    std::uint32_t stepsExecuted = 0;
    double acceptedElapsedSeconds = 0.0;
    double simulatedSeconds = 0.0;
    double remainingDebtSeconds = 0.0;
    double discardedSeconds = 0.0;
    double totalDiscardedSeconds = 0.0;
    bool catchUpLimited = false;
};

/*
    Owns the fixed-step lifecycle of the local authoritative server.

    The runner is deliberately synchronous for now. Its only external runtime
    dependency is the server-side transport endpoint; it never sees the client
    transport interface or client state. This is the execution contract that a
    later threaded/dedicated runner will keep.
*/
class ServerRunner
{
public:
    ServerRunner(
        GameServer& server,
        IServerTransport& transport,
        ServerTickPolicy policy = {}
    );

    ServerAdvanceResult advance(double elapsedSeconds);

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
    void receiveInboundMessages();
    void publishOutboundMessages();

    GameServer& m_server;
    IServerTransport& m_transport;
    ServerTickPolicy m_policy;

    double m_accumulatorSeconds = 0.0;
    double m_totalDiscardedSeconds = 0.0;
};
}
