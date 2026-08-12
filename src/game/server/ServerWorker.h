#pragma once

#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

#include "src/game/server/ServerRunner.h"
#include "src/world/WorldParams.h"

class IServerTransport;

namespace game::debug
{
class IServerDebugChannel;
}

namespace game::server
{
/*
    Owns the OS thread that exclusively owns ServerRuntime/GameServer.

    Stage 2 uses a bounded one-batch pipeline. advance() waits only for the
    previously submitted batch (if it is still running), submits the current
    elapsed-time batch, and returns the previous completed result immediately.
    Therefore at most one authoritative batch is in flight: server simulation
    can overlap the current client/render frame without allowing an unbounded
    queue or silently deleting fixed-step input history.
*/
class ServerWorker final
{
public:
    ServerWorker(
        const WorldParams& worldParams,
        IServerTransport& transport,
        game::debug::IServerDebugChannel& debugChannel
    );
    ~ServerWorker();

    ServerWorker(const ServerWorker&) = delete;
    ServerWorker& operator=(const ServerWorker&) = delete;

    ServerAdvanceResult advance(double elapsedSeconds);
    double fixedStepSeconds() const;

private:
    void threadMain(
        WorldParams worldParams,
        IServerTransport& transport,
        game::debug::IServerDebugChannel& debugChannel
    );
    void stopAndJoin() noexcept;

    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_thread;

    bool m_started = false;
    bool m_stopRequested = false;
    bool m_advancePending = false;

    double m_pendingElapsedSeconds = 0.0;
    double m_fixedStepSeconds = 0.02;
    ServerAdvanceResult m_lastAdvanceResult;
    std::exception_ptr m_failure;
};
}
