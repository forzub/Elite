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

    // OWN CLOCK (audit #7/#8 D2): by default the worker advances the authoritative simulation by its own monotonic
    // clock (steady_clock, woken for every 0.02 s fixed step) - the client frame no longer drives server time, a frozen
    // or stalled client / host GUI does not stop it. advance() then only reports the steps run since the previous call
    // (non-blocking). OPT-IN: ELITE_SERVER_OWN_CLOCK=1 (default = the client-driven one-batch pipeline).
    // Explicit states: Running, Paused (single player ESC menu: the clock frozen, resumed from a fresh base - no
    // catch-up of the paused time), Overloaded (the runner hit its catch-up limit / debt cap: logged on entry and exit,
    // the ServerRunner clips stay - 0.25 s per wake, 0.5 s debt, 8 steps - so a slow tick can never spiral), Stopping.
    void setPaused(bool paused);
    bool ownClock() const noexcept { return m_ownClock; }

    bool ready() const;
    void rethrowIfFailed() const;

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
    bool m_ownClock = false;   // ELITE_SERVER_OWN_CLOCK=1 (opt-in until the client prediction is ready for it)
    bool m_paused = false;
    // own clock: what ran since the client's last advance() (summed), handed over and reset there
    ServerAdvanceResult m_sinceLastReport;

    double m_pendingElapsedSeconds = 0.0;
    double m_fixedStepSeconds = 0.02;
    ServerAdvanceResult m_lastAdvanceResult;
    std::exception_ptr m_failure;
};
}
