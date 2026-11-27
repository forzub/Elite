#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include "src/game/server/ServerWorker.h"

#include <chrono>
#include <functional>
#include <utility>

#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/IServerTransport.h"
#include "src/game/server/ServerRuntime.h"

namespace game::server
{
ServerWorker::ServerWorker(
    const WorldParams& worldParams,
    IServerTransport& transport,
    game::debug::IServerDebugChannel& debugChannel
)
{
    // Construction is intentionally non-blocking. ServerRuntime still belongs
    // exclusively to this worker thread, but a local NEW GAME must not freeze
    // the Win32/WebView thread while the authoritative world loads its static
    // data. LocalGameSession polls ready()/rethrowIfFailed() during its normal
    // loading-state update instead.
    // the own clock is OPT-IN again (the author 2026-11-27: "the ship oscillates strongly in flight, especially in
    // turns" right after it became the default): the client's prediction / reconciliation still assumes one server
    // batch per client frame. ELITE_SERVER_OWN_CLOCK=1 enables it for the investigation.
    if (const char* v = std::getenv("ELITE_SERVER_OWN_CLOCK"); v && *v == '1')
        m_ownClock = true;
    std::cerr << "[ServerClock] " << (m_ownClock ? "own monotonic clock (ELITE_SERVER_OWN_CLOCK=1)"
                                                 : "client-driven (default)") << std::endl;
    m_thread = std::thread(
        &ServerWorker::threadMain,
        this,
        worldParams,
        std::ref(transport),
        std::ref(debugChannel)
    );
}

ServerWorker::~ServerWorker()
{
    stopAndJoin();
}

bool ServerWorker::ready() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_started && !m_failure;
}

void ServerWorker::rethrowIfFailed() const
{
    std::exception_ptr failure;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        failure = m_failure;
    }

    if (failure)
        std::rethrow_exception(failure);
}

namespace
{
// DEV (audit #6 D2): ELITE_DEV_WORKER_TIMING=1 - every 5 s the split of the client / worker threads: the client's wait
// for the previous batch (back-pressure), the worker's busy (one batch) and idle time, the elapsed it was handed, the
// overlap (busy / wall). Low overhead: plain samples, printed by the worker.
struct WorkerTiming
{
    std::mutex m;
    std::vector<double> clientWaitS, workerIdleS, busyS, elapsedS;
    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
};
WorkerTiming& workerTiming()
{
    static WorkerTiming t;
    return t;
}
void reportWorkerTiming(double busyS, double elapsedS)
{
    static const bool on = [] {
        const char* v = std::getenv("ELITE_DEV_WORKER_TIMING");
        return v && *v == '1';
    }();
    auto& t = workerTiming();
    t.busyS.push_back(busyS);
    t.elapsedS.push_back(elapsedS);
    const double window = std::chrono::duration<double>(std::chrono::steady_clock::now() - t.windowStart).count();
    if (window < 5.0)
        return;
    if (on)
    {
        const auto stats = [](std::vector<double> v) {
            if (v.empty())
                return std::string("n/a");
            std::sort(v.begin(), v.end());
            double sum = 0.0;
            for (double x : v)
                sum += x;
            char b[160];
            std::snprintf(b, sizeof b, "sum %.0f ms, mean %.2f, p95 %.2f, max %.2f ms (n %zu)", sum * 1000.0,
                          sum * 1000.0 / v.size(), v[static_cast<std::size_t>(0.95 * (v.size() - 1))] * 1000.0, v.back() * 1000.0,
                          v.size());
            return std::string(b);
        };
        double busy = 0.0;
        for (double x : t.busyS)
            busy += x;
        std::fprintf(stderr, "[WorkerTiming] %.1f s window | client waits for the previous batch: %s | worker busy: %s | worker idle: "
                             "%s | elapsed handed: %s | worker busy share %.0f %%\n",
                     window, stats(t.clientWaitS).c_str(), stats(t.busyS).c_str(), stats(t.workerIdleS).c_str(),
                     stats(t.elapsedS).c_str(), 100.0 * busy / window);
    }
    t.clientWaitS.clear();
    t.workerIdleS.clear();
    t.busyS.clear();
    t.elapsedS.clear();
    t.windowStart = std::chrono::steady_clock::now();
}
} // namespace

ServerAdvanceResult ServerWorker::advance(double elapsedSeconds)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_failure)
        std::rethrow_exception(m_failure);

    if (!m_started)
        return m_lastAdvanceResult;

    if (m_ownClock)
    {
        // the worker keeps its own time: hand over what ran since the last call
        ServerAdvanceResult report = m_sinceLastReport;
        m_sinceLastReport = ServerAdvanceResult {};
        m_sinceLastReport.totalDiscardedSeconds = report.totalDiscardedSeconds;
        (void)elapsedSeconds;
        return report;
    }

    // Keep the pipeline bounded to exactly one authoritative batch. If the
    // previous batch outlived the client/render frame, back-pressure is applied
    // here before accepting more elapsed time. We never queue an unbounded list
    // of frame deltas and never skip the fixed-step input stream to catch up.
    const auto clientWaitStart = std::chrono::steady_clock::now();
    m_condition.wait(lock, [this]() {
        return !m_advancePending || static_cast<bool>(m_failure);
    });
    workerTiming().clientWaitS.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now() - clientWaitStart).count());

    if (m_failure)
        std::rethrow_exception(m_failure);

    // Report the most recently completed authoritative batch. The batch being
    // submitted below intentionally remains in flight while the caller proceeds
    // with client prediction/presentation for this frame.
    const ServerAdvanceResult completedResult = m_lastAdvanceResult;

    m_pendingElapsedSeconds = elapsedSeconds;
    m_advancePending = true;
    m_condition.notify_all();

    return completedResult;
}

void ServerWorker::setPaused(bool paused)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_paused == paused)
            return;
        m_paused = paused;
    }
    std::cerr << "[ServerClock] " << (paused ? "PAUSED (single player menu)" : "RESUMED") << std::endl;
    m_condition.notify_all();
}

double ServerWorker::fixedStepSeconds() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fixedStepSeconds;
}

void ServerWorker::threadMain(
    WorldParams worldParams,
    IServerTransport& transport,
    game::debug::IServerDebugChannel& debugChannel)
{
    try
    {
        // Runtime is a thread-local lifetime owner, not a member reachable by
        // LocalGameHost. It is destroyed on the same worker that constructed
        // and advanced it.
        ServerRuntime runtime(worldParams, transport, debugChannel);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_fixedStepSeconds = runtime.fixedStepSeconds();
            m_started = true;
        }
        m_condition.notify_all();

        if (m_ownClock)
        {
            using Clock = std::chrono::steady_clock;
            const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(runtime.fixedStepSeconds()));
            auto last = Clock::now();
            bool overloaded = false;
            while (true)
            {
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    if (m_paused && !m_stopRequested)
                    {
                        m_condition.wait(lock, [this]() { return m_stopRequested || !m_paused; });
                        last = Clock::now();   // resumed: a fresh base - the paused time is not simulated
                    }
                    if (m_stopRequested)
                        break;
                    const auto idleStart = Clock::now();
                    m_condition.wait_until(lock, last + step, [this]() { return m_stopRequested || m_paused; });
                    workerTiming().workerIdleS.push_back(std::chrono::duration<double>(Clock::now() - idleStart).count());
                    if (m_stopRequested)
                        break;
                    if (m_paused)
                        continue;
                }
                const auto now = Clock::now();
                const double elapsedSeconds = std::chrono::duration<double>(now - last).count();
                last = now;
                ServerAdvanceResult result = runtime.advance(elapsedSeconds);
                result.executionWallSeconds = std::chrono::duration<double>(Clock::now() - now).count();
                reportWorkerTiming(result.executionWallSeconds, elapsedSeconds);
                // OVERLOADED: explicit, logged on entry / exit (the runner's clips keep it bounded)
                const bool over = result.catchUpLimited || result.discardedSeconds > 0.0;
                if (over != overloaded)
                {
                    overloaded = over;
                    std::cerr << "[ServerClock] " << (over ? "OVERLOADED" : "recovered") << ": wake " << elapsedSeconds * 1000.0
                              << " ms, " << result.stepsExecuted << " step(s), debt " << result.remainingDebtSeconds * 1000.0
                              << " ms, dropped now " << result.discardedSeconds * 1000.0 << " ms, dropped total "
                              << result.totalDiscardedSeconds * 1000.0 << " ms" << std::endl;
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_lastAdvanceResult = result;
                    auto& acc = m_sinceLastReport;
                    acc.stepsExecuted += result.stepsExecuted;
                    acc.acceptedElapsedSeconds += result.acceptedElapsedSeconds;
                    acc.simulatedSeconds += result.simulatedSeconds;
                    acc.discardedSeconds += result.discardedSeconds;
                    acc.executionWallSeconds += result.executionWallSeconds;
                    acc.remainingDebtSeconds = result.remainingDebtSeconds;
                    acc.totalDiscardedSeconds = result.totalDiscardedSeconds;
                    acc.catchUpLimited = acc.catchUpLimited || result.catchUpLimited;
                }
            }
            return;
        }

        while (true)
        {
            double elapsedSeconds = 0.0;
            const auto idleStart = std::chrono::steady_clock::now();
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this]() {
                    return m_stopRequested || m_advancePending;
                });
                workerTiming().workerIdleS.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now() - idleStart).count());

                if (m_stopRequested && !m_advancePending)
                    break;

                elapsedSeconds = m_pendingElapsedSeconds;
            }

            const auto executionStart = std::chrono::steady_clock::now();
            ServerAdvanceResult result = runtime.advance(elapsedSeconds);
            result.executionWallSeconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - executionStart
            ).count();
            reportWorkerTiming(result.executionWallSeconds, elapsedSeconds);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastAdvanceResult = result;
                m_advancePending = false;
            }
            m_condition.notify_all();
        }
    }
    catch (...)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_failure = std::current_exception();
            m_advancePending = false;
        }
        m_condition.notify_all();
    }
}

void ServerWorker::stopAndJoin() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = true;
    }
    m_condition.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}
}
